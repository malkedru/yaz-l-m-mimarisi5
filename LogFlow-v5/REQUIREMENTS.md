# Artim 5 gereksinim karsiliklari

| Gereksinim | Gerceklestirim / dogrulama |
| --- | --- |
| Kurtarilabilir StageException / durduran PipelineException | Stage.h; ConfigError kalitimi, I/O siniflari, ARCHITECTURE.md hata modeli |
| Asama bazinda yapilandirilabilir politikalar | ErrorHandling.h ErrorPolicy ve PolicyStage; loader onError semasi |
| FAIL_FAST | dirty-fail-fast.json; ilk kayitta durma ve aborted sayaci testi |
| SKIP | dirty-skip.json; 11 okunan, 3 cikan, 8 atlanan |
| DEAD_LETTER | dirty-dead-letter.json; bagimsiz DeadLetterSink ve 8 tanili kayit |
| RETRY(n) | onError nesnesi; ilk deneme + n tekrar; basari, tukenme, sifir butce ve fatal bypass testleri |
| FailedRecord(rawInput, stageName, exception, timestamp) | ErrorHandling.h; gecersiz UTF-8 icin tersinir bayt kodlamasi |
| Ikinci bagimsiz cikti yolu ve mimari diyagram | ARCHITECTURE.md Artim 5 Mermaid diyagrami; ayri yasam dongusu |
| Zorunlu alanlar | ValidationStage required; eksik alan testi |
| Sayisal araliklar | ValidationStage ranges; uc degerler, negatif ve ust sinir testleri |
| Zaman tutarliligi | Yapilandirilabilir UTC epoch min/max; 1970 ve 2099 dead-letter'a gider |
| IP bicimi | IPv4/IPv6 dogrulama; gecerli ve hatali adres testleri |
| Dogrulama hatalarinin politikaya gitmesi | PolicyStage ile ayni hata kanali; fixture'da 2 validation hatasi |
| RunSummary | Okunan, cikan, asama basina atilan, DL, tekrar, aborted, monotonik sure |
| Her calismanin sonunda ozet | runWithSummary; basari/hata/bos girdi/yeniden calistirma testleri |
| Bozuk fixture | ZIP'ten baytlari korunarak data/access-dirty.log ve access-empty.log |
| Kesilmis satirlar | 2 satir parse DEAD_LETTER |
| Gecersiz UTF-8 | 0xFF 0xFE 0xFA reddi ve ham baytlarin geri alinabilirligi testi |
| Negatif byte | CLF parse hatasi ve dead-letter; ValidationStage icin ayrica negatif deger testi |
| 1970 / 2099 | validate.timestamp araligi disinda; DEAD_LETTER |
| 100 KiB satir | parse.maxLineBytes=65536; DEAD_LETTER; ozgun 102400 bayt korunur |
| Bos dosya | 0 sayac, bos iki cikti, basari |
| Dogru ana cikti | 3 gecerli kaydin tam TSV icerigi karsilastirilir |
| Tam sayac dengesi | 11 = 3 + 0 + 8; aciklama satiri da acikca sayilir |
| Hata turu / politika tablosu | ARCHITECTURE.md Artim 5 tablosu |
| v5 etiketli surum | VERSION, --version, --help ve Git v5 etiketi |
| Onceki ozelliklerin korunmasi | Parser, filtre, projeksiyon, sink, builder, CLI, loader ve entegrasyon testleri |
| Testlerin gercek basarisizlikta durmasi | Windows build.ps1 negatif islem cikis kodlarini da kontrol eder |

Calistirma komutu: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Test`.
