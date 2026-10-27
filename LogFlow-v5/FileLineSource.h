#ifndef FILELINESOURCE_H
#define FILELINESOURCE_H

#include "Source.h"
#include "Stage.h"
#include <fstream>
#include <string>

class FileLineSource : public Source<std::string> {
private:
    std::string filePath;

public:
    FileLineSource(const std::string& path) : filePath(path) {}

    void produce(Emitter<std::string>& out) override {
        std::ifstream file(filePath.c_str(), std::ios::binary);

        if (!file.is_open()) {
            throw PipelineException("Dosya acilamadi: " + filePath);
        }

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            out.emit(line);
        }
        if (file.bad()) throw PipelineException("Girdi dosyasi okunamadi: " + filePath);

        file.close();
    }
};

#endif
