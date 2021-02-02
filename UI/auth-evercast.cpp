
#include "auth-evercast.hpp"
#include "remote-text.hpp"
#include "obs-app.hpp"

EvercastAuth::BaseUrlAndPath EvercastAuth::parseUrlComponents(const std::string& url) {

	BaseUrlAndPath result;

        int pos = -1;
        for(unsigned int i = 1; i < url.length(); i++) {
                if(url[i] == '/' && url[i - 1] != ':' && url[i - 1] != '/') {
                        pos = i;
                        break;
                }
        }

        if(pos == -1) {
                result.baseUrl = url;
                result.path = "/";
        } else {
                result.baseUrl = url.substr(0, pos);
                result.path = url.substr(pos, url.length());
        }

	return result;
}

void EvercastAuth::skipChar(const std::string& text, int& pos, char c) {
        while(pos < text.length() && text[pos] == c) {
                pos ++;
        }
}

bool EvercastAuth::findChar(const std::string& text, int& pos, char c) {
        while(pos < text.length() && text[pos] != c) {
                pos ++;
        }
        return pos < text.length() && text[pos] == c;
}

std::string EvercastAuth::parseValue(const std::string& text,  const std::string& key) {

        int pos = 0;

        int keyPos = -1;
        int valuePos = -1;

        std::string value = "";

        if(findChar(text, pos, ':')) {
		pos ++;
	}

        while(pos < text.length()) {
                if(keyPos == -1 && valuePos == -1) {
                        skipChar(text, pos, ' ');
                        keyPos = pos;
                } else if(keyPos != -1 && valuePos == -1) {
                        if(!findChar(text, pos, '=')) break;
                        std::string currKey(&text[keyPos], pos - keyPos);
                        if(currKey == key) {
                                pos ++;
                                keyPos = -1;
                                valuePos = pos;
                        } else {
                                if(!findChar(text, pos, ';')) break;
                                pos ++;
                                keyPos = -1;
                                valuePos = -1;
                        }
                } else if(keyPos == -1 && valuePos != -1) {
                        if(!findChar(text, pos, ';')) break;
                        value = std::string(&text[valuePos], pos - valuePos);
                        break;
                }
        }

        return value;

}

EvercastAuth::Token EvercastAuth::getTokenInfoFromCookies(const std::vector<std::string>& headers) {

        Token result;

        for(auto& h : headers) {

                auto token = parseValue(h, "__Host-jwt");
                auto nonce = parseValue(h, "__Host-nonce");

                if(!token.empty()) result.token = token;
                if(!nonce.empty()) result.nonce = nonce;

        }

        return result;

}

json11::Json EvercastAuth::createLoginQuery(const Credentials& credentials) {

        std::string query = "mutation authenticateMutation($input: AuthenticateInput!) {authenticate(input: $input) {clientMutationId}}";

        json11::Json j = json11::Json::object {
                {"query", query },
                {"variables", json11::Json::object {
                        {"input", json11::Json::object {
                                { "email", credentials.email },
                                { "password", credentials.password },
                                { "trackingId", credentials.trackingId }
                        }}
                }}
        };

        return j;

}

json11::Json EvercastAuth::createStreamKeyQuery() {

        std::string query = "mutation CreateStreamKeyMutation(\n $input : CreateStreamKeyInput !\n) {\n createStreamKey(input : $input) {\n uuid\n } \n }\n";

        json11::Json j = json11::Json::object {
                {"query", query },
                {"variables", json11::Json::object {
                        {"input", json11::Json::object {}}
                }}
        };

        return j;

}

json11::Json EvercastAuth::obtainStreamKeyQuery() {

        std::string query = "mutation getStreamKeyMutation(\n  $input: GetStreamKeyInput!\n) {\n  getStreamKey(input: $input) {\n    uuid\n  }\n}\n";

        json11::Json j = json11::Json::object {
                {"query", query },
                {"variables", json11::Json::object {
                        {"input", json11::Json::object {}}
                }}
        };

        return j;

}

json11::Json EvercastAuth::createRoomsQuery() {

        std::string query = "query homeQuery {\n"
		"\tcurrentProfile {\n"
		"\t\t...Home_currentProfile\n"
		"\t}\n"
		"\tcanCreateRoom\n"
		"}\n"
		"\n"
		"fragment Home_currentProfile on Profile {\n"
		"\n"
		"\trooms: liveroomsByCreatorId(first: 100, orderBy: [CREATED_AT_DESC]) {\n"
		"\t\tnodes {\n"
		"\t\t\tid\n"
		"\t\t\thash\n"
		"\t\t\tcreatedAt\n"
		"\t\t\tdeletedAt\n"
		"\t\t\t...RoomCard_room\n"
		"\t\t}\n"
		"\t}\n"
		"\tinvites: invitesByProfileId(first: 100, orderBy: [CREATED_AT_DESC]) {\n"
		"\t\tnodes {\n"
		"\t\t\tliveroomByRoomId {\n"
		"\t\t\t\tid\n"
		"\t\t\t\tcreatedAt\n"
		"\t\t\t\tdeletedAt\n"
		"\t\t\t\t...RoomCard_room\n"
		"\t\t\t\tprofileByCreatorId {\n"
		"\t\t\t\t\tdisplayName\n"
		"\t\t\t\t}\n"
		"\t\t\t}\n"
		"\t\t}\n"
		"\t}\n"
		"\trecentRooms: joinedRoomsByProfileId(first: 100, orderBy: [JOINED_AT_DESC]) {\n"
		"\t\tnodes {\n"
		"\t\t\tjoinedAt\n"
		"\t\t\tliveroomByRoomId {\n"
		"\t\t\t\tid\n"
		"\t\t\t\tcreatedAt\n"
		"\t\t\t\tdeletedAt\n"
		"\t\t\t\t...RoomCard_room\n"
		"\t\t\t}\n"
		"\t\t}\n"
		"\t}\n"
		"}\n"
		"\n"
		"fragment RoomCard_room on Liveroom {\n"
		"\tid\n"
		"\thash\n"
		"\tdisplayName\n"
		"\tcreatorId\n"
		"\tcreatedAt\n"
		"\tsessionCount\n"
		"\tnoteCount\n"
		"}";

        json11::Json j = json11::Json::object{
                {"query", query},
        };

        return j;

}

EvercastAuth::HttpResponse EvercastAuth::execHttp(const std::string& url,
						  const std::string& body,
						  const std::vector<std::string>& headers)
{

        HttpResponse res;

        GetRemoteFileAndHeaders(url.c_str(), res.body, res.error,
                                &res.code, "application/json", body.c_str(),
                                headers, &res.headers, 5);

	return res;

}

EvercastAuth::Token EvercastAuth::obtainToken(const Credentials& credentials) {

        std::string apiURL = config_get_string(GetGlobalConfig(), "General", "evercast_url_graphql");
        blog(LOG_INFO, "EvercastAuth::obtainToken(). apiURL='%s'", apiURL.c_str());

        auto query = createLoginQuery(credentials);
        auto res = execHttp(apiURL, query.dump());

        if(!res.error.empty()) {
                blog(LOG_INFO, "error='%s'", res.error.c_str());
		return {};
	}

	auto tokenInfo = getTokenInfoFromCookies(res.headers);
        blog(LOG_INFO, "OK: nonce='%s', token='%s'", tokenInfo.nonce.c_str(), tokenInfo.token.c_str());

        return tokenInfo;

}

std::string EvercastAuth::obtainStreamKey(const Token& token) {

        std::string apiURL = config_get_string(GetGlobalConfig(), "General", "evercast_url_graphql");
        blog(LOG_INFO, "EvercastAuth::obtainStreamKey(). apiURL='%s'", apiURL.c_str());

        auto query = obtainStreamKeyQuery();
        auto res = execHttp(apiURL, query.dump(),
			    {
				    "X-Double-Submit: " + token.nonce,
				    "cookie: __Host-nonce=" + token.nonce + "; __Host-jwt=" + token.token
			    });

        if(!res.error.empty()) {
                blog(LOG_INFO, "error='%s'", res.error.c_str());
		return "";
        }

        std::string err;
        auto j = json11::Json::parse(res.body, err);
        if (!err.empty()) {
                blog(LOG_INFO, "error='%s'", err.c_str());
		return "";
	}

        auto key = j["data"]["getStreamKey"]["uuid"].string_value();
        blog(LOG_INFO, "streamKey='%s'", key.c_str());

        return key;

}

std::string EvercastAuth::createStreamKey(const Token& token) {

        std::string apiURL = config_get_string(GetGlobalConfig(), "General", "evercast_url_graphql");
        blog(LOG_INFO, "EvercastAuth::createStreamKey(). apiURL='%s'", apiURL.c_str());

        auto query = createStreamKeyQuery();
        auto res = execHttp(apiURL, query.dump(),
                            {
                                    "X-Double-Submit: " + token.nonce,
                                    "cookie: __Host-nonce=" + token.nonce + "; __Host-jwt=" + token.token
                            });

        if(!res.error.empty()) {
                blog(LOG_INFO, "error='%s'", res.error.c_str());
        }

        std::string err;
        auto j = json11::Json::parse(res.body, err);
        if (!err.empty()) {
                blog(LOG_INFO, "error='%s'", err.c_str());
                return "";
        }

        auto key = j["data"]["createStreamKey"]["uuid"].string_value();
        blog(LOG_INFO, "streamKey='%s'", key.c_str());

        return key;
}

EvercastAuth::Rooms EvercastAuth::obtainRooms(const Token& token) {

        std::string apiURL = config_get_string(GetGlobalConfig(), "General", "evercast_url_graphql");
        blog(LOG_INFO, "EvercastAuth::obtainRooms(). apiURL='%s'", apiURL.c_str());

        auto query = createRoomsQuery();
        auto res = execHttp(apiURL, query.dump(),
                            {
                                    "X-Double-Submit: " + token.nonce,
                                    "cookie: __Host-nonce=" + token.nonce + "; __Host-jwt=" + token.token
                            });

        if(!res.error.empty()) {
                blog(LOG_INFO, "error='%s'", res.error.c_str());
        }

        std::string err;
        auto j = json11::Json::parse(res.body, err);
        if (!err.empty()) {
                blog(LOG_INFO, "error='%s'", err.c_str());
                return {};
        }

        std::unordered_map<std::string, Room> allRooms;

        for (auto &room : j["data"]["currentProfile"]["recentRooms"]["nodes"].array_items()) {
                auto jId =room["liveroomByRoomId"]["id"].string_value();
                auto jName = room["liveroomByRoomId"]["displayName"].string_value();
                if (!jId.empty() && !jName.empty()) {
                        Room r = {jId, jName};
                        allRooms.insert({jId, r});
                }
        }

        for (auto &room : j["data"]["currentProfile"]["invites"]["nodes"].array_items()) {
                auto jId =room["liveroomByRoomId"]["id"].string_value();
                auto jName = room["liveroomByRoomId"]["displayName"].string_value();
                if (!jId.empty() && !jName.empty()) {
                        Room r = {jId, jName};
                        allRooms.insert({jId, r});
                }
        }

        for (auto &room : j["data"]["currentProfile"]["rooms"]["nodes"].array_items()) {
                auto jId = room["id"].string_value();
                auto jName = room["displayName"].string_value();
                if (!jId.empty() && !jName.empty()) {
                        Room r = {jId, jName};
                        allRooms.insert({jId, r});
                }
        }

        Rooms rooms;

        for(auto& pair : allRooms) {
                rooms.ordered.push_back(pair.second);
        }

        sort(rooms.ordered.begin(), rooms.ordered.end(), []( const Room& r1, const Room& r2 ) {
                return r1.name < r2.name;
        });

        return rooms;

}

void EvercastAuth::updateState() {

	auto token = getToken();
	if(token.empty()) {
		/* get token if it's empty */
                token = obtainToken(getCredentials());
		setToken(token);
                if(token.empty()) {
                        return;
                }
	}

	auto streamKey = obtainStreamKey(token);
	if(streamKey.empty()) {
		/* get a new token, old one was revoked */
                token = obtainToken(getCredentials());
                setToken(token);
		if(token.empty()) {
                        setStreamKey("");
			return;
		}
		streamKey = obtainStreamKey(token);

		if (streamKey.empty()) {
			/* Still no stream key; try creating one */
			streamKey = createStreamKey(token);
		}
	}
        setStreamKey(streamKey);
        if(streamKey.empty()) {
                return;
        }

	setRooms(obtainRooms(token));

}

void EvercastAuth::loadState(obs_data_t *settings) {

	Credentials credentials;
        credentials.email = obs_data_get_string(settings, "evercast_auth_email");
        credentials.trackingId = obs_data_get_string(settings, "evercast_auth_tracking_id");

	Token token;
	token.token = obs_data_get_string(settings, "evercast_auth_token");
	token.nonce = obs_data_get_string(settings, "evercast_auth_token_nonce");

	std::string streamKey;
	streamKey = obs_data_get_string(settings, "evercast_auth_stream_key");

	setCredentials(credentials);
	setToken(token);
	setStreamKey(streamKey);
}

void EvercastAuth::saveState(obs_data_t *settings) {

	const auto& credentials = getCredentials();
        const auto& token = getToken();
        const auto& streamKey = getStreamKey();

        obs_data_set_string(settings, "evercast_auth_email", credentials.email.c_str());
        obs_data_set_string(settings, "evercast_auth_tracking_id", credentials.trackingId.c_str());

        obs_data_set_string(settings, "evercast_auth_token", token.token.c_str());
        obs_data_set_string(settings, "evercast_auth_token_nonce", token.nonce.c_str());

        obs_data_set_string(settings, "evercast_auth_stream_key", streamKey.c_str());

}

void EvercastAuth::clearCurrentState() {
	setCredentials({getCredentials().email, "", getCredentials().trackingId});
	setToken({});
	setStreamKey("");
	setRooms({});
}

void EvercastAuth::setCredentials(const Credentials& credentials) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_credentials = credentials;
}

EvercastAuth::Credentials EvercastAuth::getCredentials() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_credentials;
}

void EvercastAuth::setToken(const Token& token) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_token = token;
}

EvercastAuth::Token EvercastAuth::getToken() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_token;
}

void EvercastAuth::setStreamKey(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_streamKey = key;
}

std::string EvercastAuth::getStreamKey() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_streamKey;
}

void EvercastAuth::setRooms(const Rooms& rooms) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_rooms = rooms;
}

EvercastAuth::Rooms EvercastAuth::getRooms() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_rooms;
}
