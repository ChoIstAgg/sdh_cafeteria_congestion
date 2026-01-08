#include "crow.h"
#include <iostream>
#include <mutex>

long latest_distance = -1;
int current_congestion = -1;
std::mutex data_mutex;

int main() {
    crow::SimpleApp app;

    // 단순 테스트용 (센서 1개)
    CROW_ROUTE(app, "/test").methods("POST"_method)(
      [](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");

      {
        std::lock_guard<std::mutex> lock(data_mutex);
        latest_distance = x["distance"].i();
      }
        std::cout << "[TEST] 수신 거리: " << latest_distance << " cm" << std::endl;
        return crow::response(200, "Test Success");
    });

    // POST 라우트 설정
    CROW_ROUTE(app, "/measure").methods("POST"_method)(
      [](const crow::request& req) {
      auto x = crow::json::load(req.body);

      // JSON 파싱 확인
      if (!x) { return crow::response(400, "Invalid JSON"); }


      std::lock_guard<std::mutex> lock(data_mutex);
      latest_distance = x["distance"].i();

      std::cout << "\n[데이터 수신]" << std::endl;
      std::cout << " - 거리: " << latest_distance << " cm" << std::endl;
      std::cout << " - 상태: " << (latest_distance < 20 ? "매우 혼잡" : "여유") << std::endl;

      return crow::response(200, "Data received successfully");
    });

    // 웹페이지 데이터 수신 헤더
    CROW_ROUTE(app, "/status")(
      [](const crow::request& req) {
        crow::json::wvalue x; 

        {
            std::lock_guard<std::mutex> lock(data_mutex);
            x["distance"] = latest_distance;
            x["congestion"] = current_congestion;
        } 

        crow::response res(std::move(x));
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
    
        return res;
      }
    );

    app.ssl_file("serverkeys/server.crt", "serverkeys/server.key");
    app.port(443).run();
}
