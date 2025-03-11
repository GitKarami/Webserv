#include "../includes/Request.hpp"
#include <sstream>

Request::Request() {}
Request::~Request() {}
Request::Request(const std::string& request)
{
	parse(request);
}

std::string Request::getHeader(const std::string& key) const
{
	auto it = _headers.find(key);
	if (it == _headers.end())
		return "";
	return it->second;
}

void Request::parse(const std::string& request)
{
	size_t				headerEnd;
	std::string			headerLines;
	std::string			header;
	std::string			body;
	std::string			line;

	if (request.empty())
		return;
	headerEnd = request.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
	{	
		headerEnd = request.find("\n\n");
		if (headerEnd == std::string::npos)
			return;
	}
	header = request.substr(0, headerEnd);
	body = "";
	if (headerEnd + 4 < request.size())
		body = request.substr(headerEnd + 4);
	else if (headerEnd + 2 < request.size())
		body = request.substr(headerEnd + 2);
	std::istringstream	headerStream(header);
	if (std::getline(headerStream, line))
	{
		if (line.back() == '\r')
			line.pop_back();
		parseRequestLine(line);
	}
	while (std::getline(headerStream, line))
	{
		if (line.back() == '\r')
			line.pop_back();
		
	}
	parseHeader(headerLines);
	parseBody(body);
	std::cout << "parsed request" << std::endl;
	std::cout << "method: " << _method << std::endl;
	std::cout << "path: " << _path << std::endl;
	std::cout << "version: " << _version << std::endl;
	for (auto& header : _headers)
		std::cout << header.first << ": " << header.second << std::endl;
	std::cout << "body: " << _body << std::endl;
}

void Request::parseHeader(const std::string& headers)
{
    std::istringstream iss(headers);
    std::string line;
    
    while (std::getline(iss, line))
    {
        if (line.empty() || line == "\r")
            continue;
            
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos)
        {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            // Trim leading spaces in value
            size_t valueStart = value.find_first_not_of(" \t");
            if (valueStart != std::string::npos)
                value = value.substr(valueStart);
                
            // Store header
            _headers[key] = value;
        }
    }
}

void Request::parseBody(const std::string& body)
{
	_body = body;
}
void Request::parseRequestLine(const std::string& request)
{
	std::istringstream iss(request);
	iss >> _method >> _path >> _version;
}

std::string Request::getMethod() const {return _method;}
std::string Request::getPath() const	{return _path;}
std::string Request::getVersion() const	{return _version;}
std::string Request::getBody() const	{return _body;}
