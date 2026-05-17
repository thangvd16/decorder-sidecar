#include "ZXing/ZXingCpp.h"

#include <chrono>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
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
          escaped.push_back(hex[(ch >> 4) & 0x0F]);
          escaped.push_back(hex[ch & 0x0F]);
        } else {
          escaped.push_back(ch);
        }
    }
  }

  return escaped;
}

static void decode_image(const std::string& path)
{
  const GrayImage image = load_pgm(path);
  const ZXing::ImageView view(image.pixels.data(), image.width, image.height, ZXing::ImageFormat::Lum);
  const auto barcodes = ZXing::ReadBarcodes(view);
  const int64_t timestamp = unix_timestamp_ms();

  std::cout << "{\"results\":[";
  bool first = true;
  for (const auto& barcode : barcodes) {
    if (!barcode.isValid()) {
      continue;
    }
    if (!first) {
      std::cout << ",";
    }
    first = false;
    std::cout << "{\"text\":\"" << json_escape(barcode.text()) << "\",\"format\":\""
              << json_escape(ZXing::ToString(barcode.format())) << "\",\"timestamp\":" << timestamp << "}";
  }
  std::cout << "],\"timestamp\":" << timestamp << "}\n";
}

static void print_usage()
{
  std::cerr << "Usage: pack-audit-decoder --decode-image <fixture.pgm>\n";
}

int main(int argc, char** argv)
{
  try {
    if (argc == 3 && std::string(argv[1]) == "--decode-image") {
      decode_image(argv[2]);
      return 0;
    }

    print_usage();
    return 2;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
