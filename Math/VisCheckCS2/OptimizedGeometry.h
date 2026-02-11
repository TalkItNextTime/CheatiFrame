#pragma once
#include <string>
#include <vector>
#include "Math.hpp"

class OptimizedGeometry {
public:
    // Каждый элемент – меш (список треугольников)
    std::vector<std::vector<TriangleCombined>> meshes;

    // Загружает оптимизированные данные из бинарного файла .opt
    bool LoadFromFile(const std::string& optimizedFile);

    // Создаёт оптимизированный файл из сырого .vphys файла,
    // извлекая нужные данные с помощью Parser, и сохраняет результат в optimizedFile.
    bool CreateOptimizedFile(const std::string& rawFile, const std::string& optimizedFile);
};
