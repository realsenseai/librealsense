// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// assistant_chat_client member functions grouped here for convenience (not a separate class): the
// two one-shot requests below already run on their own thread and own curl handle, untouched by
// _curl/_busy/_cancel_requested, which is why they can live apart from the streaming lifecycle.

#ifdef ENABLE_AI_ASSISTANT

#include "assistant-chat-client.h"
#include "assistant-chat-config.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <rsutils/string/from.h>
#include <thread>

namespace rs2
{
    namespace assistant
    {
        // Hand-rolled rather than reusing rs2::http::http_downloader: that class is compiled only
        // under CHECK_FOR_UPDATES, not ENABLE_AI_ASSISTANT, so calling it here would silently report
        // "unhealthy" (its dummy stub always returns false) whenever only the assistant flag is on.
        void assistant_chat_client::check_health(invoke_fn invoke, std::function<void(bool)> on_result)
        {
            auto me = shared_from_this();
            std::thread t([me, invoke, on_result]() {
                bool healthy = false;
                try
                {
                    if (auto* curl = curl_easy_init())
                    {
                        std::string url = std::string(BASE_URL) + "/api/health";
                        std::string response;
                        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SEC);
                        curl_easy_setopt(curl, CURLOPT_TIMEOUT, ONE_SHOT_TIMEOUT_SEC);
                        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                            +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
                                static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
                                return size * nmemb;
                            });
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

                        auto res = curl_easy_perform(curl);
                        long status = 0;
                        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
                        curl_easy_cleanup(curl);
                        healthy = (res == CURLE_OK && status == 200);
                    }
                }
                catch (...)
                {
                    healthy = false;
                }
                safe_invoke(invoke, [on_result, healthy]() { on_result(healthy); });
            });
            t.detach();
        }

        void assistant_chat_client::send_reaction(const std::string& conversation_id, int value,
            invoke_fn invoke, error_callback on_error)
        {
            auto me = shared_from_this();
            std::thread t([me, conversation_id, value, invoke, on_error]() {
                try
                {
                    if (auto* curl = curl_easy_init())
                    {
                        rsutils::json body_json;
                        body_json["conversationId"] = conversation_id;
                        body_json["value"] = value;
                        std::string body = body_json.dump();

                        std::string url = std::string(BASE_URL) + "/api/reactions";
                        struct curl_slist* headers = nullptr;
                        headers = curl_slist_append(headers, "Content-Type: application/json");
                        headers = curl_slist_append(headers, "X-RS-Integration: viewer");

                        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                        curl_easy_setopt(curl, CURLOPT_POST, 1L);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
                        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SEC);
                        curl_easy_setopt(curl, CURLOPT_TIMEOUT, ONE_SHOT_TIMEOUT_SEC);
                        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

                        auto res = curl_easy_perform(curl);
                        long status = 0;
                        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
                        curl_slist_free_all(headers);
                        curl_easy_cleanup(curl);

                        if (res != CURLE_OK || status >= 400)
                        {
                            std::string message_text = rsutils::string::from()
                                << "Couldn't send feedback (HTTP " << status << ").";
                            safe_invoke(invoke, [on_error, message_text]() { on_error(message_text); });
                        }
                    }
                }
                catch (const std::exception& ex)
                {
                    std::string what = ex.what();
                    safe_invoke(invoke, [on_error, what]() { on_error(what); });
                }
            });
            t.detach();
        }
    }
}

#endif
