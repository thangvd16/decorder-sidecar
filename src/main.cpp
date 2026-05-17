#include <zbar.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace zbar_api = zbar;

struct GrayImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;
};

struct ZbarImageDeleter {
  void operator()(zbar_api::zbar_image_t* image) const
  {
    if (image != nullptr) {
      zbar_api::zbar_image_destroy(image);
    }
  }
};

struct ZbarProcessorDeleter {
  void operator()(zbar_api::zbar_processor_t* processor) const
  {
    if (processor != nullptr) {
      zbar_api::zbar_processor_destroy(processor);
    }
  }
};

using ZbarImagePtr = std::unique_ptr<zbar_api::zbar_image_t, ZbarImageDeleter>;
using ZbarProcessorPtr = std::unique_ptr<zbar_api::zbar_processor_t, ZbarProcessorDeleter>;

static std::string next_pnm_token(std::istream& input)
{
  std::string token;
  char ch = 0;

  while (input.get(ch)) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      continue;
    }
    if (ch == '#') {
      std::string ignored;
      std::getline(input, ignored);
      continue;
    }
    token.push_back(ch);
    break;
  }

  while (input.get(ch)) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      break;
    }
    if (ch == '#') {
      std::string ignored;
      std::getline(input, ignored);
      break;
    }
    token.push_back(ch);
  }

  if (token.empty()) {
    throw std::runtime_error("PGM header is missing data");
  }
  return token;
}

static GrayImage load_pgm(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Cannot open fixture image");
  }

  const std::string magic = next_pnm_token(file);
  const int width = std::stoi(next_pnm_token(file));
  const int height = std::stoi(next_pnm_token(file));
  const int max_value = std::stoi(next_pnm_token(file));

  if ((magic != "P5" && magic != "P2") || width <= 0 || height <= 0 || max_value <= 0 || max_value > 255) {
    throw std::runtime_error("Invalid PGM fixture");
  }

  GrayImage image;
  image.width = width;
  image.height = height;
  image.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

  if (magic == "P5") {
    file.read(reinterpret_cast<char*>(image.pixels.data()), static_cast<std::streamsize>(image.pixels.size()));
    if (file.gcount() != static_cast<std::streamsize>(image.pixels.size())) {
      throw std::runtime_error("PGM fixture is missing pixels");
    }
    return image;
  }

  for (uint8_t& pixel : image.pixels) {
    const int value = std::stoi(next_pnm_token(file));
    pixel = static_cast<uint8_t>((value * 255) / max_value);
  }

  return image;
}

static int64_t unix_timestamp_ms()
{
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

static unsigned long fourcc_y800()
{
  return zbar_fourcc('Y', '8', '0', '0');
}

static std::string json_escape(std::string_view value)
{
  std::string escaped;
  escaped.reserve(value.size() + 8);

  for (const char ch : value) {
    switch (ch) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          escaped += "\\u00";
          constexpr char hex[] = "0123456789abcdef";
          escaped.push_back(hex[(static_cast<unsigned char>(ch) >> 4U) & 0x0F]);
          escaped.push_back(hex[static_cast<unsigned char>(ch) & 0x0F]);
        } else {
          escaped.push_back(ch);
        }
    }
  }

  return escaped;
}

static std::string format_name(zbar_api::zbar_symbol_type_t type)
{
  const char* name = zbar_api::zbar_get_symbol_name(type);
  return name == nullptr ? "UNKNOWN" : name;
}

static std::string processor_error_message(zbar_api::zbar_processor_t* processor, std::string_view fallback)
{
  const char* message = zbar_api::zbar_processor_error_string(processor, 0);
  if (message == nullptr || message[0] == '\0') {
    return std::string(fallback);
  }
  return message;
}

static void print_decode_json(zbar_api::zbar_processor_t* processor, const GrayImage& image)
{
  if (image.width <= 0 || image.height <= 0) {
    throw std::runtime_error("Invalid image size");
  }
  if (image.pixels.size() > static_cast<size_t>(std::numeric_limits<unsigned long>::max())) {
    throw std::runtime_error("Image payload is too large for ZBar");
  }

  ZbarImagePtr zbar_image(zbar_api::zbar_image_create());
  if (zbar_image == nullptr) {
    throw std::runtime_error("Cannot allocate ZBar image");
  }

  zbar_api::zbar_image_set_format(zbar_image.get(), fourcc_y800());
  zbar_api::zbar_image_set_size(zbar_image.get(), static_cast<unsigned>(image.width), static_cast<unsigned>(image.height));
  zbar_api::zbar_image_set_data(zbar_image.get(), image.pixels.data(), static_cast<unsigned long>(image.pixels.size()), nullptr);

  const int64_t timestamp = unix_timestamp_ms();
  if (zbar_api::zbar_process_image(processor, zbar_image.get()) < 0) {
    throw std::runtime_error(processor_error_message(processor, "ZBar failed to process image"));
  }

  std::cout << "{\"results\":[";
  bool first = true;
  for (const zbar_api::zbar_symbol_t* symbol = zbar_api::zbar_image_first_symbol(zbar_image.get()); symbol != nullptr; symbol = zbar_api::zbar_symbol_next(symbol)) {
    const char* data = zbar_api::zbar_symbol_get_data(symbol);
    const zbar_api::zbar_symbol_type_t type = zbar_api::zbar_symbol_get_type(symbol);
    if (data == nullptr || type == zbar_api::ZBAR_PARTIAL) {
      continue;
    }

    if (!first) {
      std::cout << ",";
    }
    first = false;
    std::cout << "{\"text\":\"" << json_escape(data) << "\",\"format\":\""
              << json_escape(format_name(type)) << "\",\"timestamp\":" << timestamp << "}";
  }
  std::cout << "],\"timestamp\":" << timestamp << "}\n";
}

static ZbarProcessorPtr create_processor()
{
  ZbarProcessorPtr processor(zbar_api::zbar_processor_create(0));
  if (processor == nullptr) {
    throw std::runtime_error("Cannot allocate ZBar processor");
  }

  if (zbar_api::zbar_processor_init(processor.get(), nullptr, 0) != 0) {
    throw std::runtime_error(processor_error_message(processor.get(), "Cannot initialize ZBar processor"));
  }

  if (zbar_api::zbar_processor_set_config(processor.get(), zbar_api::ZBAR_NONE, zbar_api::ZBAR_CFG_ENABLE, 1) != 0) {
    throw std::runtime_error("Cannot enable ZBar decoder symbologies");
  }

  return processor;
}

static void decode_image(const std::string& path)
{
  ZbarProcessorPtr processor = create_processor();
  const GrayImage image = load_pgm(path);
  print_decode_json(processor.get(), image);
}

static void decode_stdin()
{
  ZbarProcessorPtr processor = create_processor();

  std::string command;
  while (std::cin >> command) {
    if (command != "FRAME") {
      throw std::runtime_error("Expected FRAME command");
    }

    int width = 0;
    int height = 0;
    size_t byte_length = 0;
    std::cin >> width >> height >> byte_length;
    if (!std::cin || width <= 0 || height <= 0) {
      throw std::runtime_error("Invalid frame header");
    }

    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (byte_length != expected || byte_length > static_cast<size_t>(std::numeric_limits<int>::max())) {
      throw std::runtime_error("Frame byte length must match width * height");
    }

    const int separator = std::cin.get();
    if (separator != '\n') {
      throw std::runtime_error("Frame header must end with newline");
    }

    GrayImage image;
    image.width = width;
    image.height = height;
    image.pixels.resize(byte_length);

    std::cin.read(reinterpret_cast<char*>(image.pixels.data()), static_cast<std::streamsize>(byte_length));
    if (std::cin.gcount() != static_cast<std::streamsize>(byte_length)) {
      throw std::runtime_error("Frame payload is incomplete");
    }

    if (std::cin.peek() == '\n') {
      std::cin.get();
    }

    print_decode_json(processor.get(), image);
    std::cout.flush();
  }
}

static void print_usage()
{
  std::cerr << "Usage:\n"
            << "  pack-audit-decoder --decode-image <fixture.pgm>\n"
            << "  pack-audit-decoder --decode-stdin\n";
}

int main(int argc, char** argv)
{
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif

  try {
    if (argc == 3 && std::string(argv[1]) == "--decode-image") {
      decode_image(argv[2]);
      return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "--decode-stdin") {
      decode_stdin();
      return 0;
    }

    print_usage();
    return 2;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
