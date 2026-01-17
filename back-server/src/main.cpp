#include "crow.h"
#include <iostream>
#include <mutex>

// 혼잡도 데이터만 저장 (0~100 사이의 값)
int current_congestion = -1; 
std::mutex data_mutex;

int main() {
    crow::SimpleApp app;

    // 1. 데이터 수신용 POST 라우트 (/measure)
    CROW_ROUTE(app, "/measure").methods("POST"_method)(
      [](const crow::request& req) {
        auto x = crow::json::load(req.body);

        // JSON 파싱 확인
        if (!x) { 
            return crow::response(400, "Invalid JSON"); 
        }

        {
            std::lock_guard<std::mutex> lock(data_mutex);
            // JSON에서 congestion 키의 값을 읽어 정수로 저장
            current_congestion = x["congestion"].i();
        }

        std::cout << "\n[데이터 수신]" << std::endl;
        std::cout << " - 현재 혼잡도: " << current_congestion << "%" << std::endl;

        return crow::response(200, "Congestion data received");
    });

    // 2. 프론트엔드 데이터 제공용 GET 라우트 (/status)
    CROW_ROUTE(app, "/status")(
      [](const crow::request& req) {
        crow::json::wvalue x;

        {
            std::lock_guard<std::mutex> lock(data_mutex);
            x["congestion"] = current_congestion;
        }

        crow::response res(std::move(x));
        // CORS 문제 해결을 위한 헤더 설정
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");

        return res;
      }
    );

    // 포트 50001에서 서버 실행
    app.port(50001).run();
}
