#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

#include "openucc/compiler.hpp"
#include "openucc/resources.hpp"
#include "openucc/target.hpp"

using namespace openucc;
namespace {
std::string json(const std::string& s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += char(c);
    } else if (c < 32) {
      const char* hex = "0123456789abcdef";
      out += "\\u00";
      out += hex[c >> 4];
      out += hex[c & 15];
    } else
      out += char(c);
  }
  return out + '"';
}
void summary(const Package& p) {
  std::cout << "{\"version\":" << p.version << ",\"licensee\":" << p.licensee
            << ",\"envelope\":" << json(p.envelope)
            << ",\"names\":" << p.names.size()
            << ",\"imports\":" << p.imports.size()
            << ",\"exports\":" << p.exports.size() << ",\"classes\":[";
  bool first = true;
  for (std::size_t i = 0; i < p.exports.size(); ++i)
    if (!p.exports[i].type) {
      if (!first) std::cout << ',';
      first = false;
      std::cout << json(p.ref(int(i) + 1));
    }
  std::cout << "],\"dependencies\":[";
  first = true;
  for (const auto& item : p.imports) {
    if (item.outer != 0 || p.name(item.type) != "Package") continue;
    if (!first) std::cout << ',';
    first = false;
    std::cout << json(p.name(item.name));
  }
  std::cout << "]}\n";
}
void equivalent(const Package& a, const Package& b) {
  if (a.version != b.version || a.licensee != b.licensee ||
      a.flags != b.flags || a.names.size() != b.names.size() ||
      a.imports.size() != b.imports.size() ||
      a.exports.size() != b.exports.size())
    throw std::runtime_error("Roundtrip summary mismatch");
  for (std::size_t i = 0; i < a.names.size(); ++i)
    if (a.names[i].text != b.names[i].text ||
        a.names[i].flags != b.names[i].flags)
      throw std::runtime_error("Roundtrip name mismatch");
  for (std::size_t i = 0; i < a.imports.size(); ++i) {
    auto& x = a.imports[i];
    auto& y = b.imports[i];
    if (x.package != y.package || x.type != y.type || x.outer != y.outer ||
        x.name != y.name)
      throw std::runtime_error("Roundtrip import mismatch");
  }
  for (std::size_t i = 0; i < a.exports.size(); ++i) {
    auto& x = a.exports[i];
    auto& y = b.exports[i];
    auto payload = x.payload;
    for (const auto& relocation : x.lazyOffsets) {
      if (relocation.first > payload.size() ||
          payload.size() - relocation.first < 4 ||
          relocation.second > payload.size() || y.sourceOffset > 0x7fffffffU ||
          relocation.second > 0x7fffffffU - y.sourceOffset)
        throw std::runtime_error("Invalid roundtrip resource relocation");
      auto value = y.sourceOffset + relocation.second;
      for (int byte = 0; byte < 4; ++byte)
        payload[relocation.first + byte] = std::uint8_t(value >> (byte * 8));
    }
    if (x.type != y.type || x.super != y.super || x.outer != y.outer ||
        x.name != y.name || x.flags != y.flags || payload != y.payload)
      throw std::runtime_error("Roundtrip export mismatch");
  }
}
}  // namespace
int runMain(int argc, char** argv) {
  try {
    if (argc == 3 && std::string(argv[1]) == "inspect") {
      summary(Package::load(std::filesystem::u8path(argv[2])));
      return 0;
    }
    if (argc >= 3 && std::string(argv[1]) == "model") {
      TargetModel model;
      for (int i = 2; i < argc; ++i) {
        auto path = std::filesystem::u8path(argv[i]);
        model.add(path.stem().u8string(), Package::load(path));
      }
      std::size_t classes = 0, functions = 0, properties = 0, failures = 0;
      std::vector<std::string> issues;
      for (std::size_t k = 0; k < model.packages.size(); ++k) {
        auto& p = model.packages[k].data;
        for (std::size_t i = 0; i < p.exports.size(); ++i) {
          int id = int(i) + 1;
          try {
            auto kind = p.kind(id);
            if (kind == "Class") {
              model.ancestry({k, id});
              ++classes;
            } else if (kind == "Function") {
              functionInfo(p, id);
              ++functions;
            } else if (kind.size() >= 8 &&
                       kind.substr(kind.size() - 8) == "Property") {
              propertyInfo(p, id);
              ++properties;
            }
          } catch (const std::exception& e) {
            ++failures;
            if (issues.size() < 100)
              issues.push_back(model.identity({k, id}) + ": " + e.what());
          }
        }
      }
      std::cout << "{\"packages\":" << model.packages.size()
                << ",\"classes\":" << classes << ",\"functions\":" << functions
                << ",\"properties\":" << properties
                << ",\"failures\":" << failures << ",\"issues\":[";
      for (std::size_t i = 0; i < issues.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << json(issues[i]);
      }
      std::cout << "]}\n";
      return failures ? 1 : 0;
    }
    if (argc == 4 && std::string(argv[1]) == "rebuild-resources") {
      const auto inputPath = std::filesystem::u8path(argv[2]);
      const auto outputPath = std::filesystem::u8path(argv[3]);
      const auto original = readFile(inputPath);
      const auto source = Package::parseFile(original, inputPath);
      auto output = source;
      std::vector<std::string> warnings;
      for (std::size_t i = 0; i < source.exports.size(); ++i) {
        int id = int(i) + 1;
        try {
          auto body = decodeResource(source, id);
          if (body.modelOpaque) {
            warnings.push_back(source.ref(id) + " [Model]: " + body.modelWarning);
            continue;
          }
          encodeResource(body, output, output.exports[i], [](int ref) { return ref; });
        } catch (const std::exception& e) {
          throw std::runtime_error(source.ref(id) + " [" + source.kind(id) + "]: " + e.what());
        }
      }
      if (!warnings.empty()) {
        // Unknown Model bytes may contain package references or absolute
        // offsets. Preserve the entire original file instead of presenting
        // a potentially corrupted relocation as a reconstructed package.
        if (readFile(inputPath) != original)
          throw std::runtime_error("Input changed during Model preservation");
        writeNewFile(outputPath, original);
        std::cerr << "WARNING: Unverified Model layout; the entire original package was "
                     "copied unchanged. No resources were reconstructed or relocated.\n";
        if (source.envelope == "Lineage2Ver121" &&
            lower(inputPath.filename().u8string()) != lower(outputPath.filename().u8string()))
          std::cerr << "WARNING: Ver121 encryption depends on the filename; use the original "
                       "filename to read the unchanged copy.\n";
        std::cout << "{\"status\":\"unchanged_copy\",\"reconstruction_performed\":false,\"warnings\":[";
        for (std::size_t i = 0; i < warnings.size(); ++i) {
          if (i) std::cout << ',';
          std::cout << json(warnings[i]);
        }
        std::cout << "]}\n";
        return 0;
      }
      auto bytes = output.serialize();
      auto checked = Package::parse(bytes);
      equivalent(output, checked);
      writeNewFile(outputPath, bytes);
      summary(checked);
      return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "roundtrip") {
      auto p = Package::load(std::filesystem::u8path(argv[2]));
      auto bytes = p.serialize();
      auto q = Package::parse(bytes);
      equivalent(p, q);
      if (argc == 4) writeNewFile(std::filesystem::u8path(argv[3]), bytes);
      summary(q);
      return 0;
    }
    if (argc >= 5 && (std::string(argv[1]) == "compile" ||
                      std::string(argv[1]) == "compile-objects")) {
      const bool objects = std::string(argv[1]) == "compile-objects";
      TargetModel target;
      target.add("Core", Package::load(std::filesystem::u8path(argv[2])));
      std::vector<std::pair<std::string, Bytes>> sources;
      bool traceProgress = false;
      for (int i = 4; i < argc; ++i) {
        if (std::string(argv[i]) == "--trace-progress") {
          traceProgress = true;
        } else if (std::string(argv[i]) == "--reference") {
          if (++i == argc) throw std::runtime_error("Missing reference path");
          auto path = std::filesystem::u8path(argv[i]);
          target.addReference(path.stem().u8string(), Package::load(path));
        } else if (std::string(argv[i]) == "--dependency") {
          if (++i == argc) throw std::runtime_error("Missing dependency path");
          auto path = std::filesystem::u8path(argv[i]);
          target.add(path.stem().u8string(), Package::load(path));
        } else if (std::string(argv[i]) == "--source-dir") {
          if (++i == argc) throw std::runtime_error("Missing source directory");
          auto directory = std::filesystem::u8path(argv[i]);
          std::size_t before = sources.size();
          for (const auto& entry :
               std::filesystem::recursive_directory_iterator(directory)) {
            const auto& path = entry.path();
            if (entry.is_regular_file() && lower(path.extension().u8string()) ==
                                               (objects ? ".uobj" : ".uc"))
              sources.push_back({path.lexically_relative(directory).u8string(),
                                 readFile(path)});
          }
          if (sources.size() == before)
            throw std::runtime_error(objects
                                         ? "No .uobj files in source directory"
                                         : "No .uc files in source directory");
        } else {
          auto path = std::filesystem::u8path(argv[i]);
          sources.push_back({path.filename().u8string(), readFile(path)});
        }
      }
      auto outputPath = std::filesystem::u8path(argv[3]);
      for (auto& dep : target.packages)
        if (dep.referenceOnly &&
            lower(dep.name) != lower(outputPath.stem().u8string()))
          throw std::runtime_error("Reference must name the output package");
        else if (!dep.referenceOnly &&
                 lower(dep.name) == lower(outputPath.stem().u8string()))
          throw std::runtime_error(
              "Output package conflicts with a dependency");
      std::sort(sources.begin(), sources.end(),
                [](const auto& a, const auto& b) {
                  return lower(a.first) < lower(b.first);
                });
      auto started = std::chrono::steady_clock::now();
      std::function<void(const std::string&)> progress;
      if (traceProgress)
        progress = [&](const std::string& stage) {
          auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - started)
                        .count();
          std::cerr << "[" << ms << " ms] " << stage << std::endl;
        };
      auto outputName = outputPath.stem().u8string();
      auto p = objects ? compileObjects(sources, target, outputName, progress)
                       : compile(sources, target, outputName, progress);
      if (progress) progress("serialize");
      auto bytes = p.serialize();
      if (progress) progress("roundtrip");
      equivalent(p, Package::parse(bytes));
      if (progress) progress("write");
      writeNewFile(outputPath, bytes);
      summary(p);
      return 0;
    }
    std::cerr
        << "openUCC 0.1 (in development)\n  openucc inspect input.u\n  "
           "openucc model dependency.u [dependency.u ...]\n  openucc "
           "roundtrip input.u [new-output.u]\n  openucc rebuild-resources input.utx new-output.utx\n  openucc compile Core.u "
           "new-output.u [--dependency package.u] [--reference original.u] "
           "[--source-dir directory] [--trace-progress] [source.uc ...]\n"
           "  openucc compile-objects Core.u new-output.u --reference "
           "original.u "
           "[--dependency package.u] [--source-dir directory] [source.uobj "
           "...]\n";
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "openUCC: " << e.what() << '\n';
    return 1;
  }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
  std::vector<std::string> storage;
  for (int i = 0; i < argc; ++i)
    storage.push_back(std::filesystem::path(argv[i]).u8string());
  std::vector<char*> args;
  for (auto& s : storage) args.push_back(s.data());
  return runMain(argc, args.data());
}
#else
int main(int argc, char** argv) { return runMain(argc, argv); }
#endif
