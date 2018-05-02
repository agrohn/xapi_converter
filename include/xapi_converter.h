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


std::string base64_encode(const unsigned char *src, size_t len);
