#pragma once

#include <obs-module.h>
#include <json11.hpp>

#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>

class EvercastAuth {
public:

	struct BaseUrlAndPath {
		std::string baseUrl;
		std::string path;
	};

	struct Credentials {
                std::string email;
		std::string password;
		std::string trackingId;
	};

        struct Token {

                std::string token;
                std::string nonce;

		bool empty() const {
			return token.empty();
		}

        };

	struct Room {
		std::string id;
		std::string name;
	};

	struct Rooms {
                std::vector<Room> ordered;
	};

private:

	struct HttpResponse {
                long code;
		std::string body;
		std::string error;
		std::vector<std::string> headers;
	};

private:

	static HttpResponse execHttp(const std::string& url,
                                     const std::string& body,
                                     const std::vector<std::string>& headers = std::vector<std::string>({}));

        static BaseUrlAndPath parseUrlComponents(const std::string& url);
        static void skipChar(const std::string& text, int& pos, char c);
        static bool findChar(const std::string& text, int& pos, char c);
        static std::string parseValue(const std::string& text,  const std::string& key);
        static Token getTokenInfoFromCookies(const std::vector<std::string>& headers);

private:

        static json11::Json createLoginQuery(const Credentials& credentials);
        static json11::Json createStreamKeyQuery();
        static json11::Json obtainStreamKeyQuery();
        static json11::Json createRoomsQuery();

        static Token obtainToken(const Credentials& credentials);
	static std::string createStreamKey(const Token& token);
        static std::string obtainStreamKey(const Token& token);
        static Rooms obtainRooms(const Token& token);

	void updateState();

private:
        std::mutex m_mutex;
        Credentials m_credentials;
	Token m_token;
	std::string m_streamKey;
	Rooms m_rooms;
public:

        void loadState(obs_data_t *settings);
	void saveState(obs_data_t *settings);

	void clearCurrentState();

	template<typename Callback>
	void updateState(Callback callback) {

                std::thread t([this, callback]{
			updateState();
			callback();
		});

		t.detach();

	}

	void setCredentials(const Credentials& credentials);

	Credentials getCredentials();

	void setToken(const Token& token);

        Token getToken();

	void setStreamKey(const std::string& key);

	std::string getStreamKey();

	void setRooms(const Rooms& rooms);

	Rooms getRooms();

};
