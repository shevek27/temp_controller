#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>


namespace fs = std::filesystem;

// read temperature form sysfs

float read_cpu_temp(const std::string& filepath)
{
    std::ifstream temp_file(filepath);
    if (!temp_file.is_open())
    {
        std::cerr << "error! cant open temp file at " << filepath << std::endl;
        return -1;
    }

    int mili_degrees;
    temp_file >> mili_degrees;
    temp_file.close();

    int degrees = mili_degrees / 1000.0;
    return degrees;
}

std::vector<std::string> find_temperature_files()
{
    std::vector<std::string> temp_files;

    for (const auto& entry : fs::directory_iterator("/sys/class/hwmon/"))
    {
        for (const auto& file : fs::directory_iterator(entry.path()))
        {
            if (file.path().filename().string().find("temp") != std::string::npos &&
            file.path().filename().string().find("_input") != std::string::npos)
            {
                temp_files.push_back(file.path().string());
            }
        }
    }
    
    return temp_files;
}


int main()
{
    std::vector<std::string> temp_files = find_temperature_files();

    if (temp_files.empty())
    {
        std::cerr << "error: no cpu temperature sensors found, problem with lm-sensors?" << std::endl;
        return 1;
    }

    for (const auto& file : temp_files)
    {
        float temperature = read_cpu_temp(file);
        if (temperature >= 0)
        {
            std::cout << "sensor: " << file << " | temperature: " << temperature << "°C" << std::endl;
        }
    }
    return 0;
}