#ifndef CAMERA_SETTINGS_H
#define CAMERA_SETTINGS_H 1

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <utils/picojson.h>
#include <opencv2/opencv.hpp>

using namespace picojson;

struct CameraSettings
{
    std::string name    = "";
    std::string source  = "";
    std::string cameraBackend = "";
    bool rs_alignToColor = false;
    int exposure   = -488;
    int contrast   = -488;
    int brightness = -488;
    int gain       = -488;
    int saturation = -488;
    int whiteBalance = -488;
    float rs_decimationMagnitude = -1.0;

    bool load(const std::string &path)
    {
        std::ifstream configFileStream(path);
        if (configFileStream.fail())
        {
            std::cerr << "ERROR: Failed to open config file: " << path << std::endl;
            return false;
        }

        // Read all the text from the config file.
        std::string jsonStr(
            (std::istreambuf_iterator<char>(configFileStream)),
            std::istreambuf_iterator<char>());
        value json;
        std::string err = parse(json, jsonStr);
        if (!err.empty())
        {
            std::cerr << err << std::endl;
            return false;
        }

        //Parse the JSON.
        name            = json.get("Name").to_str();
        source          = json.get("Source").to_str();
        cameraBackend   = json.get("CameraBackend").to_str();
        rs_alignToColor = getBoolOrDefault(json, "RS_AlignToColor");
        exposure        = getIntOrDefault(json, "Exposure");
        contrast        = getIntOrDefault(json, "Contrast");
        brightness      = getIntOrDefault(json, "Brightness");
        gain            = getIntOrDefault(json, "Gain");
        saturation      = getIntOrDefault(json, "Saturation");
        whiteBalance    = getIntOrDefault(json, "WhiteBalance");
        rs_decimationMagnitude = getFloatOrDefault(json, "RS_DecimationMagnitude");

        return true;
    }

    bool save(const std::string &path)
    {
        std::ofstream outputFileStream(path);
        if (outputFileStream.fail())
        {
            std::cerr << "ERROR: Failed to open file for saving camera settings: " << path << std::endl;
            return false;
        }

        // Format the camera data.
        outputFileStream << "{ \n";
        outputFileStream << "\t\"Name\": " << "\"" << name << "\"" << "\n";
        outputFileStream << "\t\"Source\": " << "\"" << source << "\"" << "\n";
        outputFileStream << "\t\"RS_AlignToColor\": " << "\"" << rs_alignToColor << "\"" << "\n";
        if (exposure != -488)
            outputFileStream << "\t\"Exposure\": "   << exposure   << ",\n";
        if (contrast != -488)
            outputFileStream << "\t\"Contrast\": "   << contrast   << ",\n";
        if (brightness != -488)
            outputFileStream << "\t\"Brightness\": " << brightness << ",\n";
        if (gain != -488)
            outputFileStream << "\t\"Gain\": "       << gain       << ",\n";
        if (saturation != -488)
            outputFileStream << "\t\"Saturation\": " << saturation << ",\n";
        if (!cameraBackend.empty())
            outputFileStream << "\t\"CameraBackend\": " << "\"" << cameraBackend << "\"" << ",\n";
        if (whiteBalance != -488)
            outputFileStream << "\t\"WhiteBalance\": " << whiteBalance << ",\n";
        if (rs_decimationMagnitude != -1.0)
            outputFileStream << "\t\"RS_DecimationMagnitude\": " << rs_decimationMagnitude << ",\n";
        outputFileStream << "}";

        return true;
    }

    private:
        int getIntOrDefault(const value &json, const std::string &name, int def = -488)
        {
            try
            {
                return stoi(json.get(name).to_str());
            }
            catch(const std::exception& e)
            {
                return def;
            }
        }

        float getFloatOrDefault(const value &json, const std::string &name, float def = -1.0)
        {
            try
            {
                return stof(json.get(name).to_str());
            }
            catch(const std::exception& e)
            {
                return def;
            }
        }

        bool to_bool(std::string str) {
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            std::istringstream is(str);
            bool b;
            is >> std::boolalpha >> b;
            return b;
        }

        bool getBoolOrDefault(const value &json, const std::string &name, bool def = false)
        {
            try
            {
                return to_bool(json.get(name).to_str());
            }
            catch(const std::exception& e)
            {
                return def;
            }
            
        }
};

#endif //ifndef CAMERA_SETTINGS_H
