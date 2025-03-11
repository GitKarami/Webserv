#pragma once

#include <string>
#include <iostream>
#include <map>


class Response
{
	private:
		int _statusCode;
		std::string _statusMessage;
		std::map<std::string, std::string> _headers;
		std::string _body;

	public:
		Response();
		Response(int statusCode);
		~Response();

		int											getStatusCode() const;
		std::string									getStatusText() const;
		std::string									getHeader(const std::string& key) const;
		const std::map<std::string, std::string>&	getHeaders() const;
		std::string									getBody() const;
		std::string									getResponse() const;

		void		setStatusCode(int code);
		void		setStatusMessage(const std::string& message);
		void		setHeader(const std::string& key, const std::string& value);
		void		setBody(const std::string& body);

		std::string toString() const;
		std::string getStatusTextForCode(int code) const;
};