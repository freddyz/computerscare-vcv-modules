#pragma once

#include <osdialog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

#include "Computerscare.hpp"

static const char* COMPUTERSCARE_IMAGE_FILTERS = "Images:png,jpg,jpeg,bmp,gif";

static bool computerscareIsSupportedImagePath(const std::string& path) {
  std::string ext = system::getExtension(path);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
         ext == ".gif";
}

static std::string computerscareOpenImageDialog(const char* dir) {
  osdialog_filters* filters =
      osdialog_filters_parse(COMPUTERSCARE_IMAGE_FILTERS);
  char* pathC = osdialog_file(OSDIALOG_OPEN, dir, NULL, filters);
  osdialog_filters_free(filters);
  if (!pathC) return "";

  std::string path = pathC;
  std::free(pathC);
  return computerscareIsSupportedImagePath(path) ? path : "";
}
