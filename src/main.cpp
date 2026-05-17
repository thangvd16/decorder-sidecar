#include <zbar.h>

#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct GrayImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;
};

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
  return static_cast<unsigned long>('Y') |
         (static_cast<unsigned long>('8') << 8U) |
         (static_cast<unsigned long>('0') << 16U) |
         (static_cast<unsigned long>('0') << 24U);
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

static std::string format_name(zbar_symbol_type_t type)
{
  const char* name = zbar_get_symbol_name(type);
  return name == nullptr ? "UNKNOWN" : name;
}

static void print_decode_json(zbar_image_scanner_t* scanner, const GrayImage& image)
{
  if (image.width <= 0 || image.height <= 0) {
    throw std::runtime_error("Invalid image size");
  }

  zbar_image_t* zbar_image = zbar_image_create();
  if (zbar_image == nullptr) {
    throw std::runtime_error("Cannot allocate ZBar image");
  }

  zbar_image_set_format(zbar_image, fourcc_y800());
  zbar_image_set_size(zbar_image, static_cast<unsigned>(image.width), static_cast<unsigned>(image.height));
  zbar_image_set_data(zbar_image, image.pixels.data(), image.pixels.size(), nullptr);

  const int64_t timestamp = unix_timestamp_ms();
  zbar_scan_image(scanner, zbar_image);

  std::cout << "{\"results\":[";
  bool first = true;
  for (const zbar_symbol_t* symbol = zbar_image_first_symbol(zbar_image); symbol != nullptr; symbol = zbar_symbol_next(symbol)) {
    const char* data = zbar_symbol_get_data(symbol);
    if (data == nullptr) {
      continue;
    }

    if (!first) {
      std::cout << ",";
    }
    first = false;
    std::cout << "{\"text\":\"" << json_escape(data) << "\",\"format\":\""
              << json_escape(format_name(zbar_symbol_get_type(symbol))) << "\",\"timestamp\":" << timestamp << "}";
  }
  std::cout << "],\"timestamp\":" << timestamp << "}\n";

  zbar_image_destroy(zbar_image);
}

static zbar_image_scanner_t* create_scanner()
{
  zbar_image_scanner_t* scanner = zbar_image_scanner_create();
  if (scanner == nullptr) {
    throw std::runtime_error("Cannot allocate ZBar scanner");
  }

  zbar_image_scanner_set_config(scanner, ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
  return scanner;
}

static void decode_image(const std::string& path)
{
  zbar_image_scanner_t* scanner = create_scanner();
  const GrayImage image = load_pgm(path);
  print_decode_json(scanner, image);
  zbar_image_scanner_destroy(scanner);
}

static void decode_stdin()
{
  zbar_image_scanner_t* scanner = create_scanner();

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

    print_decode_json(scanner, image);
    std::cout.flush();
  }

  zbar_image_scanner_destroy(scanner);
}

static void print_usage()
{
  std::cerr << "Usage:\n"
            << "  pack-audit-decoder --decode-image <fixture.pgm>\n"
            << "  pack-audit-decoder --decode-stdin\n";
}

int main(int argc, char** argv)
{
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
