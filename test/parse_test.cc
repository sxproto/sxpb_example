#include <fildesh/sxproto.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int
main(int argc, char** argv)
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <source_dir>" << std::endl;
    return 1;
  }
  std::string source_dir = argv[1];
  std::string dirs_sxpb_path = source_dir + "/test/dirs.sxpb";

  FildeshX* in = open_FildeshXF(dirs_sxpb_path.c_str());
  if (!in) {
    std::cerr << "Could not open " << dirs_sxpb_path << std::endl;
    return 1;
  }

  FildeshO* stderr_out = open_FildeshOF("/dev/stderr");
  FildeshSxpb* sxpb = slurp_sxpb_close_FildeshX(in, NULL, stderr_out);
  if (!sxpb) {
    std::cerr << "Failed to parse " << dirs_sxpb_path << std::endl;
    return 1;
  }

  FildeshSxpbIT top = top_of_FildeshSxpb(sxpb);

  std::vector<std::string> dirs;
  for (FildeshSxpbIT it = first_at_FildeshSxpb(sxpb, top);
       !nullish_FildeshSxpbIT(it);
       it = next_at_FildeshSxpb(sxpb, it)) {
    dirs.push_back(str_value_at_FildeshSxpb(sxpb, it));
  }

  close_FildeshSxpb(sxpb);

  bool success = true;
  size_t golden_count = 0;
  size_t idempotent_count = 0;
  for (const auto& dir : dirs) {
    namespace fs = std::filesystem;
    fs::path dir_path = fs::path(source_dir) / dir;
    if (!fs::exists(dir_path))
      continue;

    for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
      if (entry.is_regular_file() && entry.path().extension() == ".sxpb") {
        const std::string filepath = entry.path().string();
        const auto corpus_name = entry.path().parent_path().filename();
        if (corpus_name == "golden") {
          golden_count += 1;
        } else if (corpus_name == "idempotent") {
          idempotent_count += 1;
        }

        // Validation
        FildeshX* file_in = open_FildeshXF(filepath.c_str());
        if (!file_in) {
          std::cerr << "Cannot open " << filepath << std::endl;
          success = false;
          continue;
        }
        FildeshSxpb* file_sxpb =
          slurp_sxpb_close_FildeshX(file_in, NULL, stderr_out);
        if (!file_sxpb) {
          std::cerr << "Validation failed for " << filepath << std::endl;
          success = false;
          continue;
        }

        FildeshO printed[1] = {DEFAULT_FildeshO};
        print_sxpb_FildeshO(printed, file_sxpb);
        FildeshX printed_in = FildeshX_of_bytestring(
          reinterpret_cast<const unsigned char*>(printed->at), printed->size);
        FildeshSxpb* reparsed =
          slurp_sxpb_close_FildeshX(&printed_in, NULL, stderr_out);
        if (!reparsed) {
          std::cerr << "Roundtrip reparse failed for " << filepath << std::endl;
          success = false;
        } else {
          FildeshO original_json[1] = {DEFAULT_FildeshO};
          FildeshO reparsed_json[1] = {DEFAULT_FildeshO};
          print_json_FildeshO(original_json, file_sxpb);
          print_json_FildeshO(reparsed_json, reparsed);
          if (original_json->size != reparsed_json->size ||
              std::memcmp(
                original_json->at, reparsed_json->at, original_json->size) != 0) {
            std::cerr << "Semantic roundtrip changed " << filepath << std::endl;
            success = false;
          }
          close_FildeshO(original_json);
          close_FildeshO(reparsed_json);
          close_FildeshSxpb(reparsed);
        }

        if (corpus_name == "idempotent") {
          std::ifstream source_stream(filepath, std::ios::binary);
          std::string source(
            (std::istreambuf_iterator<char>(source_stream)),
            std::istreambuf_iterator<char>());
          size_t source_size = source.size();
          size_t printed_size = printed->size;
          if (source_size > 0 && source[source_size-1] == '\n') {
            source_size -= 1;
          }
          if (printed_size > 0 && printed->at[printed_size-1] == '\n') {
            printed_size -= 1;
          }
          if (source_size != printed_size ||
              std::memcmp(source.data(), printed->at, source_size) != 0) {
            std::cerr << "Canonical print changed " << filepath << std::endl;
            success = false;
          }
        }

        std::cout << "Validated " << filepath << std::endl;
        close_FildeshO(printed);
        close_FildeshSxpb(file_sxpb);
      }
    }
  }

  if (golden_count == 0) {
    std::cerr << "No golden corpus files found" << std::endl;
    success = false;
  }
  if (idempotent_count == 0) {
    std::cerr << "No idempotent corpus files found" << std::endl;
    success = false;
  }

  close_FildeshO(stderr_out);

  return success ? 0 : 1;
}
