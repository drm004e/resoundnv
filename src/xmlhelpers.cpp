
#include "resoundnv/xmlhelpers.hpp"
#include "resoundnv/resound_exception.hpp"
#include <cstdlib>

const xmlpp::Element* get_element(const xmlpp::Node* node){
	const xmlpp::Element* nodeElement = dynamic_cast<const xmlpp::Element*>(node);
	if(nodeElement){
		return nodeElement;
	} else {
		throw Exception("Not possible to cast to an element node");
	}
}
std::string get_attribute_string(const xmlpp::Element* node, const std::string& name){
	const xmlpp::Attribute* attribute = node->get_attribute(name);
	if(attribute){
		return attribute->get_value();
	} else {
		std::string msg("Couldn't find an attribute by name : ");
		throw Exception((msg+name).c_str());
	}
}
std::string get_optional_attribute_string(const xmlpp::Element* node, const std::string& name, std::string def){
	const xmlpp::Attribute* attribute = node->get_attribute(name);
	if(attribute){
		return attribute->get_value();
	} else {
		return def;
	}
}
float get_optional_attribute_float(const xmlpp::Element* node, const std::string& name, float def){
	const xmlpp::Attribute* attribute = node->get_attribute(name);
	if(attribute){
		return atof(attribute->get_value().c_str());
	} else {
		return def;
	}
}
