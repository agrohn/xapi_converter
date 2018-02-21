#pragma once
#include <iostream>     
#include <fstream>      
#include <vector>
#include <string>
#include <algorithm>    
#include <iterator>     




#include <cstdlib>
#include <cerrno>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <boost/tokenizer.hpp>
typedef boost::tokenizer< boost::escaped_list_separator<char> > Tokenizer;

std::string base64_encode(const unsigned char *src, size_t len);
