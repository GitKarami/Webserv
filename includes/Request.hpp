#pragma once

#include <string>
#include <iostream>
#include <map>

class Request
{
	private:
		std::string _method;
		std::string _path;
		std::string _version;
		std::map<std::string, std::string> _headers;
		std::string _body;

		void parseRequestLine(const std::string& line);
		void parseHeader(const std::string& line);
		void parseBody(const std::string& line);
	public:
		Request();
		Request(const std::string& request);
		~Request();

		void parse(const std::string& line);
		std::string getMethod() const;
		std::string getPath() const;
		std::string getVersion() const;
		std::string getHeader(const std::string& key) const;
		std::string getBody() const;
};