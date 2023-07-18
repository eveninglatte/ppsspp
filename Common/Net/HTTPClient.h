#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <cstdint>

#include "Common/File/Path.h"
#include "Common/Net/NetBuffer.h"
#include "Common/Net/Resolve.h"

namespace net {

class Connection {
public:
	Connection();
	virtual ~Connection();

	// Inits the sockaddr_in.
	bool Resolve(const char *host, int port, DNSType type = DNSType::ANY);

	bool Connect(int maxTries = 2, double timeout = 20.0f, bool *cancelConnect = nullptr);
	void Disconnect();

	// Only to be used for bring-up and debugging.
	uintptr_t sock() const { return sock_; }

protected:
	// Store the remote host here, so we can send it along through HTTP/1.1 requests.
	// TODO: Move to http::client?
	std::string host_;
	int port_ = -1;

	addrinfo *resolved_ = nullptr;

private:
	uintptr_t sock_ = -1;

};

}	// namespace net

namespace http {

bool GetHeaderValue(const std::vector<std::string> &responseHeaders, const std::string &header, std::string *value);

class RequestParams {
public:
	RequestParams() {}
	explicit RequestParams(const char *r) : resource(r) {}
	RequestParams(const std::string &r, const char *a) : resource(r), acceptMime(a) {}

	std::string resource;
	const char *acceptMime = "*/*";
};

class Client : public net::Connection {
public:
	Client();
	~Client();

	// Return value is the HTTP return code. 200 means OK. < 0 means some local error.
	int GET(const RequestParams &req, Buffer *output, net::RequestProgress *progress);
	int GET(const RequestParams &req, Buffer *output, std::vector<std::string> &responseHeaders, net::RequestProgress *progress);

	// Return value is the HTTP return code.
	int POST(const RequestParams &req, const std::string &data, const std::string &mime, Buffer *output, net::RequestProgress *progress);
	int POST(const RequestParams &req, const std::string &data, Buffer *output, net::RequestProgress *progress);

	// HEAD, PUT, DELETE aren't implemented yet, but can be done with SendRequest.

	int SendRequest(const char *method, const RequestParams &req, const char *otherHeaders, net::RequestProgress *progress);
	int SendRequestWithData(const char *method, const RequestParams &req, const std::string &data, const char *otherHeaders, net::RequestProgress *progress);
	int ReadResponseHeaders(net::Buffer *readbuf, std::vector<std::string> &responseHeaders, net::RequestProgress *progress);
	// If your response contains a response, you must read it.
	int ReadResponseEntity(net::Buffer *readbuf, const std::vector<std::string> &responseHeaders, Buffer *output, net::RequestProgress *progress);

	void SetDataTimeout(double t) {
		dataTimeout_ = t;
	}

	void SetUserAgent(const std::string &value) {
		userAgent_ = value;
	}

protected:
	std::string userAgent_;
	const char *httpVersion_;
	double dataTimeout_ = 900.0;
};

enum class RequestMethod {
	GET,
	POST,
};

enum class ProgressBarMode {
	NONE,
	VISIBLE,
	DELAYED,
};

// Really an asynchronous request.
class Download {
public:
	Download(RequestMethod method, const std::string &url, const std::string &postData, const std::string &postMime, const Path &outfile, ProgressBarMode progressBarMode = ProgressBarMode::DELAYED);
	~Download();

	void SetAccept(const char *mime) {
		acceptMime_ = mime;
	}

	void SetUserAgent(const std::string &userAgent) {
		userAgent_ = userAgent;
	}

	void Start();

	void Join();

	// Returns 1.0 when done. That one value can be compared exactly - or just use Done().
	float Progress() const { return progress_.progress; }
	float SpeedKBps() const { return progress_.kBps; }

	bool Done() const { return completed_; }
	bool Failed() const { return failed_; }

	// NOTE! The value of ResultCode is INVALID until Done() returns true.
	int ResultCode() const { return resultCode_; }

	std::string url() const { return url_; }
	const Path &outfile() const { return outfile_; }

	// If not downloading to a file, access this to get the result.
	Buffer &buffer() { return buffer_; }
	const Buffer &buffer() const { return buffer_; }

	void Cancel() {
		cancelled_ = true;
	}

	bool IsCancelled() const {
		return cancelled_;
	}

	// NOTE: Completion callbacks (which these are) are deferred until RunCallback is called. This is so that
	// the call will end up on the thread that calls g_DownloadManager.Update().
	void SetCallback(std::function<void(Download &)> callback) {
		callback_ = callback;
	}
	void RunCallback() {
		if (callback_) {
			callback_(*this);
		}
	}
	// Visual name for the download, to be displayed in progress bars.
	void SetName(const std::string &&name) { name_ = std::move(name); }

private:
	void Do();  // Actually does the download. Runs on thread.
	int Perform(const std::string &url);
	std::string RedirectLocation(const std::string &baseUrl);
	void SetFailed(int code);

	RequestMethod method_;
	net::RequestProgress progress_;
	std::string postData_;
	std::string userAgent_;
	Buffer buffer_;
	std::vector<std::string> responseHeaders_;
	std::string url_;
	Path outfile_;
	std::thread thread_;
	const char *acceptMime_ = "*/*";
	std::string postMime_;
	int resultCode_ = 0;
	bool completed_ = false;
	bool failed_ = false;
	bool cancelled_ = false;
	ProgressBarMode progressBarMode_;
	bool joined_ = false;
	std::string name_;
	std::function<void(Download &)> callback_;
};

using std::shared_ptr;

class Downloader {
public:
	~Downloader() {
		CancelAll();
	}

	std::shared_ptr<Download> StartDownload(const std::string &url, const Path &outfile, ProgressBarMode mode, const char *acceptMime = nullptr);

	std::shared_ptr<Download> StartDownloadWithCallback(
		const std::string &url,
		const Path &outfile,
		ProgressBarMode mode,
		std::function<void(Download &)> callback,
		const char *acceptMime = nullptr);

	std::shared_ptr<Download> AsyncPostWithCallback(
		const std::string &url,
		const std::string &postData,
		const std::string &postMime, // Use postMime = "application/x-www-form-urlencoded" for standard form-style posts, such as used by retroachievements. For encoding form data manually we have MultipartFormDataEncoder.
		ProgressBarMode mode,
		std::function<void(Download &)> callback);

	// Drops finished downloads from the list.
	void Update();
	void CancelAll();

	void WaitForAll();
	void SetUserAgent(const std::string &userAgent) {
		userAgent_ = userAgent;
	}

private:
	std::vector<std::shared_ptr<Download>> downloads_;
	// These get copied to downloads_ in Update(). It's so that callbacks can add new downloads
	// while running.
	std::vector<std::shared_ptr<Download>> newDownloads_;

	std::string userAgent_;
};

}	// http
