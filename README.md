# LogFlow v5 - Hata Yonetimi, Dogrulama ve Dead-Letter Kanali

C++11 ile yapilandirmadan kurulan log isleme hatti. Artim 5, kayit hatalarini asama basina secilen politikayla yonetir; gecerli kayitlari ana hedefe, bozuk kayitlari bagimsiz dead-letter hedefine gonderir. Harici C++ kutuphanesi gerekmez.

## Derleme ve test

Proje kokunde PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Test
.\build\logflow.exe --version
.\build\logflow.exe --config configs/dirty-dead-letter.json
```

VS Code: Ctrl+Shift+B derler; `LogFlow: test` gorevi butun testleri calistirir. MSVC varsa otomatik bulunur; yoksa PATH uzerindeki g++ kullanilir. MSVC C++14 secenegiyle derlenir; kaynak C++11 uyumludur. Derleme kokteki `logflow.exe` kopyasini da gunceller.

Linux/macOS/MinGW:

```sh
make
make test
./logflow --config configs/dirty-dead-letter.json
```

## 5. hafta gosterimi

| Yapilandirma | Beklenen sonuc |
| --- | --- |
| `configs/dirty-dead-letter.json` | 11 okunan, 3 ana cikti, 8 dead-letter, 0 atlanan; cikis 0 |
| `configs/dirty-skip.json` | 11 okunan, 3 ana cikti, 8 atlanan; cikis 0 |
| `configs/dirty-fail-fast.json` | Ilk satirda durur; 1 okunan, 1 aborted; cikis 1 |
| `configs/empty.json` | Butun sayaclar 0, temiz tamamlanma; cikis 0 |

```powershell
.\build\logflow.exe --config configs/dirty-dead-letter.json
Get-Content out/valid.tsv
Get-Content out/dead-letter.jsonl
.\build\logflow.exe --config configs/dirty-skip.json
.\build\logflow.exe --config configs/dirty-fail-fast.json
.\build\logflow.exe --config configs/empty.json
```

Kirli dosyada 3 gecerli ve 7 bozuk log kaydi, ayrica CLF olmayan bir aciklama satiri bulunur. Aciklama da girdi satiri olarak sayilir ve parse hatasi politikasina gider: toplam **11 = 3 + 0 + 8**. Parser 6, dogrulama 2 satiri dead-letter'a gonderir. Kaynak dosya ZIP'ten baytlari degistirilmeden alinmistir. 1970/2099 tarihlerini reddetmek icin ornekte UTC epoch araligi 2000-01-01 ile 2029-12-31 sonu secilmistir. 100 KiB satir, yapilandirilan 64 KiB sinirini asar.

## Hata politikalari ve dogrulama

Her asamaya `onError` eklenebilir. Belirtilmezse onceki yapilandirmalarla uyum icin `SKIP` kullanilir.

```json
{"type":"parse","params":{"format":"clf","maxLineBytes":65536},"onError":"DEAD_LETTER"}
```

- `FAIL_FAST`: ilk kayit hatasinda calismayi durdurur.
- `SKIP`: kaydi atlar, asama sayacini artirir, devam eder.
- `DEAD_LETTER`: ham girdi ve tani bilgisini `pipeline.deadLetter` hedefindeki JSONL dosyasina yazar, devam eder.
- `{"mode":"RETRY","retries":3}`: ilk denemeden sonra en fazla 3 ek deneme; butce bitince hatti durdurur. 0..1000 kabul edilir. Yeniden denemeler ek girdi kaydi sayilmaz. 6. haftanin gecici I/O hatalarina hazirliktir. Dis sistem yan etkilerinin idempotent olmasi asamanin sorumlulugudur.

`validate` asamasinin bildirime dayali kurallari:

```json
{
  "type":"validate",
  "params":{
    "required":["timestamp","clientIp","method","path","status"],
    "ranges":{"bytes":{"min":0,"max":1000000000},"status":{"min":100,"max":599}},
    "timestamp":{"min":946684800,"max":1893455999},
    "ip":true
  },
  "onError":"DEAD_LETTER"
}
```

Aralik uclari dahildir; zaman sinirlari UTC epoch saniyesidir. IPv4 ve IPv6 desteklenir; IPv4'te bastaki gereksiz sifirlar reddedilir. Zorunlu metin alanlari bos olamaz; `timestamp=0` ve `status=0` eksik kabul edilir, `bytes=0` gecerlidir. Kurallarin gerektirdigi alani daha once atan projeksiyon yukleme sirasinda reddedilir. Negatif byte ve hatali durum kodu CLF sozdizimi kontrolunde, ayristirilmis kayitlarin ek araliklari dogrulama asamasinda yakalanir.

Bagimsiz hedef tanimi:

```json
"deadLetter":{"type":"file","params":{"path":"out/dead-letter.jsonl"}}
```

Her satir `rawInput`, `rawEncoding`, `stageName`, `exception`, UTC `timestamp` icerir. Ham girdinin her bayti bir Latin-1 kod noktasiyla, ASCII disi degerler JSON `\u00XX` kacisiyla saklanir. Boylece gecersiz UTF-8 bile kayipsiz geri alinir. Python: `json.loads(line)["rawInput"].encode("latin-1")`. Satir sonlandiricisi ham kayda dahil degildir; CRLF sonundaki CR kaldirilir. Projeksiyon ham alani silse de calisma baglami ozgun girdiyi hata kanalina tasir.

## Calisma ozeti ve hata kodlari

`RunSummary` her baslayan calistirmanin sonunda, hata halinde de stderr'e yazilir. stdout yalnizca ana kayitlar icindir. Alanlar: `received`, `emitted`, `dropped`, `deadLettered`, `aborted`, `elapsedMs`, `completed`, `reconciled`. Her asama icin atilan, yonlendirilen, basarisiz olan, hata ve tekrar deneme sayilari bulunur. Normal filtre elemeleri de `dropped` toplamina dahildir.

Denge: `received = emitted + dropped + deadLettered + aborted`. Tamamlanan calismada `aborted=0`; durdurulan calismada henuz sonuclandirilamayan kayit burada sayilir. `emitted`, sink'in kabul ettigi kayittir; kapatma/flush hatasi olursa `completed=false` olur ve kalici yazma garantisi verilmez. Sure monotonik `steady_clock` ile olculur. C++ API: `Pipeline::summary()` (loader ile kurulan hatlarda); eski CLI ayni RunContext olcumunu kullanir. Yapilandirma yuklenemeden calisma baslamaz, dolayisiyla calisma ozeti uretilmez.

Cikis kodlari: **0** basari, **2** kullanim/yapilandirma hatasi, **1** durdurulan calisma veya I/O hatasi. Kayit hatasi `StageException`; devam edilemeyen hata `PipelineException`; `ConfigError`, `PipelineException` alt turudur. Kaynak/hedef erisimi, disk yazma ve dead-letter kanal hatalari kayit politikasiyla yutulmaz.

Tum yapilandirma I/O baslamadan dogrulanir. Girdi, ana cikti ve hata ciktisi ayni dosya olamaz. Cikti dosyalari her calismada yeniden olusturulur; ust klasorleri onceden olusturun. Goreli yollar calisma dizinine goredir. Kaynak satirlari sirayla okunur; 100 KiB satir butunuyle alinir ve parser sinirinda reddedilir. Sinirsiz buyuklukte girdiye karsi kaynak duzeyinde sabit bellek siniri bu surumde yoktur.

## Onceki kullanimlar

```powershell
.\build\logflow.exe --config configs/error-report.yaml
.\build\logflow.exe --config configs/api-traffic.json
.\build\logflow.exe --config configs/bandwidth-by-client.json
.\build\logflow.exe data/access-small.log --min-status 400 --fields timestamp,clientIp,status,path --output out/legacy.tsv
.\build\logflow.exe --help
```

Eski uc ornek sirasiyla 1, 3 ve 10 kayit uretir. JSON secilmistir; `.yaml` ornegi YAML 1.2 uyumlu JSON icerir, genel YAML ayristirma desteklenmez. `--config` eski girdi/filtre/cikti secenekleriyle karistirilamaz. Yeni kayit asamalari registry tablosuna eklenir; yeni bir topoloji icin derleme gerekmez.

Test sonucu: MSVC ile **9 test programinda 367/367 dogrulama basarili**.

## Teslim icerigi

`ARCHITECTURE.md`: hata modeli, ayri kanal diyagrami, politika ve hata turu tablosu. `REQUIREMENTS.md`: 5. hafta gereksinim eslestirmesi. `tests/ErrorHandlingTests.cpp`: kaos, tekrar deneme, hata yalitimi, dogrulama ve I/O testleri. `data/`: verilen bozuk ve bos dosyalar. `VERSION`: v5.
