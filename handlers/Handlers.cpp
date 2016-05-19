#include "handlers/Handlers.h"

#include "common/SysUtil.h"

#include "mocca/fs/Filesystem.h"
#include "mocca/log/LogManager.h"

using namespace mocca::net;

ListScenesHandler::ListScenesHandler(const mocca::fs::Path& basePath)
    : m_basePath(basePath) {}

const MethodDescription& ListScenesHandler::description() {
    static MethodDescription description("listScenes", {});
    return description;
}

Method::ReturnType ListScenesHandler::handle(const JsonCpp::Value& params) {
    Method::ReturnType result;
    auto dirContents = mocca::fs::directoryContents(m_basePath);
    for (const auto& path : dirContents) {
        if (mocca::fs::isDirectory(path)) {
            mocca::fs::Path scenePath = path + "scene.json";
            if (!mocca::fs::exists(scenePath)) {
                LERROR("Scene definition file '" << scenePath.toString() << "' does not exist");
            } else {
                JsonCpp::Reader reader;
                JsonCpp::Value root;
                std::string jsonStr = mocca::fs::readTextFile(scenePath);
                if (!reader.parse(jsonStr, root)) {
                    LERROR("Error parsing JSON: " << reader.getFormattedErrorMessages());
                } else {
                    result.first.append(root);
                }
            }
        }
    }
    return result;
}

DownloadHandler::DownloadHandler(const mocca::fs::Path& basePath)
    : m_basePath(basePath) {}

const MethodDescription& DownloadHandler::description() {
    static MethodDescription::ParameterDescription pathParam("path", JsonCpp::ValueType::stringValue, JsonCpp::Value());
    static MethodDescription description("download", {pathParam});
    return description;
}

Method::ReturnType DownloadHandler::handle(const JsonCpp::Value& params) {
    std::string sceneName = params["scene"].asString();
    mocca::fs::Path path = m_basePath + sceneName + "download" + params["filename"].asString();
    auto binary = mocca::net::MessagePart(mocca::fs::readBinaryFile(path));
    JsonCpp::Value root;
    root["type"] = "g3d"; // FIXME
    return std::make_pair(root, std::vector<mocca::net::MessagePart>{binary});
}

PythonHandler::PythonHandler(const mocca::fs::Path& basePath, const mocca::fs::Path& outputPath)
    : m_basePath(basePath)
    , m_outputPath(outputPath) {}

const mocca::net::MethodDescription& PythonHandler::description() {
    static MethodDescription::ParameterDescription pathParam("params", JsonCpp::ValueType::objectValue, JsonCpp::Value());
    static MethodDescription description("python", {pathParam});
    return description;
}

mocca::net::Method::ReturnType PythonHandler::handle(const JsonCpp::Value& params) {
    std::string sceneName = params["scene"].asString();
    mocca::fs::Path path = m_basePath + sceneName + "python" + params["filename"].asString();
    auto variables = params["variables"];
    std::vector<std::string> args;
    args.push_back(path);
    args.push_back("--variables");
    for (auto it = variables.begin(); it != variables.end(); ++it) {
        args.push_back(it.key().asString());
        args.push_back(it->asString());
    }
    args.push_back("--output");
    mocca::fs::Path outputFilePath = m_outputPath + "temp.out";
    args.push_back(outputFilePath);

    SysUtil::execute(mocca::fs::Path("python.exe"), args);

    if (!mocca::fs::exists(outputFilePath)) {
        LERROR("File '" << outputFilePath.toString() << "' does not exist");
        JsonCpp::Value root;
        return std::make_pair(root, std::vector<mocca::net::MessagePart>());
    } else {
        auto file = mocca::net::MessagePart(mocca::fs::readBinaryFile(outputFilePath));
        JsonCpp::Value root;
        return std::make_pair(root, std::vector<mocca::net::MessagePart>{file});
    }
}
