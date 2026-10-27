#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include "LogFields.h"
#include <stdexcept>
#include <string>
#include <vector>

// Hatali komut satiri kullanimi.
class UsageError : public std::runtime_error {
public:
    explicit UsageError(const std::string& message) : std::runtime_error(message) {}
};

// Komut satirindan okunan ayarlar. Filtre/projeksiyon ayarlari istege baglidir:
// verilmeyen ayar icin ilgili asama hatta hic eklenmez.
struct Options {
    std::string config;
    std::string input;
    std::string output;                   // bossa ekrana yazilir
    bool help;
    bool version;
    bool hasMinStatus;
    int minStatus;
    bool hasPathPrefix;
    std::string pathPrefix;
    std::vector<std::string> fields;      // bossa tum alanlar (projeksiyon yok)

    Options() : help(false), version(false), hasMinStatus(false), minStatus(0), hasPathPrefix(false) {}
};

class CommandLine {
private:
    static std::string takeValue(const std::vector<std::string>& args, size_t& i) {
        if (i + 1 >= args.size())
            throw UsageError(args[i] + " icin bir deger gerekli");
        return args[++i];
    }

    static int parseStatus(const std::string& text) {
        if (text.size() != 3 || text.find_first_not_of("0123456789") != std::string::npos)
            throw UsageError("--min-status 100-599 arasinda bir sayi olmali: " + text);
        int value = (text[0] - '0') * 100 + (text[1] - '0') * 10 + (text[2] - '0');
        if (value < 100 || value > 599)
            throw UsageError("--min-status 100-599 arasinda bir sayi olmali: " + text);
        return value;
    }

    static std::vector<std::string> parseFields(const std::string& text) {
        std::vector<std::string> fields;
        size_t start = 0;
        while (true) {
            size_t comma = text.find(',', start);
            std::string name = text.substr(start, comma == std::string::npos
                                                      ? std::string::npos : comma - start);
            if (!LogFields::isKnown(name))
                throw UsageError("--fields: bilinmeyen alan: '" + name + "'");
            fields.push_back(name);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        return fields;
    }

public:
    static std::string usage() {
        std::string text =
            "LogFlow v5\n"
            "Kullanim: logflow --config configs/error-report.yaml\n"
            "Kullanim: logflow <girdi.log> [secenekler]\n"
            "\n"
            "Secenekler:\n"
            "  --config DOSYA        JSON yapilandirmasindan islem hatti kurar\n"
            "  --min-status N        yalnizca durum kodu >= N olan kayitlar (100-599)\n"
            "  --path-prefix ONEK    yalnizca yolu ONEK ile baslayan kayitlar\n"
            "  --fields a,b,c        yalnizca bu alanlar yazilir (sirayla, TAB ile ayrilmis)\n"
            "  --output DOSYA        sonucu DOSYA'ya yazar (verilmezse ekrana)\n"
            "  --help                bu yardimi gosterir\n"
            "  --version             surum bilgisini gosterir\n"
            "\n"
            "Alanlar: ";
        const std::vector<std::string>& names = LogFields::names();
        for (size_t i = 0; i < names.size(); ++i)
            text += (i ? ", " : "") + names[i];
        text += "\n\nOrnek:\n"
                "  logflow data/access-small.log --min-status 400 --path-prefix /api"
                " --fields timestamp,clientIp,status --output out.tsv\n"
                "\nNot: stdout yalnizca kayitlari yazdirir; ozet bilgisi stderr'e yazilir.\n";
        return text;
    }

    // argv[0] program adidir; atlanir.
    static Options parse(int argc, const char* const argv[]) {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) args.push_back(argv[i]);

        Options opt;
        for (size_t i = 0; i < args.size(); ++i) {
            const std::string& arg = args[i];
            if (arg == "--help" || arg == "-h") {
                opt.help = true;
            } else if (arg == "--version") {
                opt.version = true;
            } else if (arg == "--config") {
                if (!opt.config.empty()) throw UsageError("--config yalnizca bir kez verilebilir");
                opt.config = takeValue(args, i);
                if (opt.config.empty() || opt.config.compare(0,2,"--")==0)
                    throw UsageError("--config icin dosya yolu gerekli");
            } else if (arg == "--min-status") {
                opt.minStatus = parseStatus(takeValue(args, i));
                opt.hasMinStatus = true;
            } else if (arg == "--path-prefix") {
                opt.pathPrefix = takeValue(args, i);
                opt.hasPathPrefix = true;
            } else if (arg == "--fields") {
                opt.fields = parseFields(takeValue(args, i));
            } else if (arg == "--output") {
                opt.output = takeValue(args, i);
            } else if (arg.compare(0, 2, "--") == 0) {
                throw UsageError("Bilinmeyen secenek: " + arg);
            } else if (opt.input.empty()) {
                opt.input = arg;
            } else {
                throw UsageError("Fazladan arguman: " + arg);
            }
        }

        if (!opt.config.empty() && (!opt.input.empty() || !opt.output.empty() || opt.hasMinStatus || opt.hasPathPrefix || !opt.fields.empty()))
            throw UsageError("--config diger girdi/filtre/cikti secenekleriyle birlestirilemez");
        if (!opt.help && !opt.version && opt.input.empty() && opt.config.empty())
            throw UsageError("Girdi dosyasi belirtilmedi");
        return opt;
    }
};

#endif
