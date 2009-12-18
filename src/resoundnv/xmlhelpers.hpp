#pragma once

#include <libxml++/libxml++.h>

// xml parsing helpers
const xmlpp::Element* get_element(const xmlpp::Node* node);
std::string get_attribute_string(const xmlpp::Element* node, const std::string& name);
std::string get_optional_attribute_string(const xmlpp::Element* node, const std::string& name, std::string def=std::string());
float get_optional_attribute_float(const xmlpp::Element* node, const std::string& name, float def=0.0f);

