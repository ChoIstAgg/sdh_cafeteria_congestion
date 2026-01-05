import React, { useEffect, useState } from 'react';

// 1. 서버 응답 데이터 타입 정의 (congestion 추가, d3 호환성 추가)
interface SensorData {
  distance?: number;
  d3?: number;       // 서버에서 d3로 보내는 경우 대비
  congestion: number;
}

const DistanceMonitor: React.FC = () => {
  const [data, setData] = useState<SensorData | null>(null);
  const [error, setError] = useState<string | null>(null);

  const fetchData = async () => {
    try {
      const response = await fetch('https://myhome-sh-1.mooo.com:50001/status');
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }
      const json = await response.json();
      setData(json);
      setError(null);
    } catch (err: any) {
      console.error("데이터 로드 실패:", err);
      // NetworkError인 경우 CORS 문제 가능성 언급
      if (err.message === "Failed to fetch" || err.name === "TypeError") {
        setError("네트워크 오류 (CORS 또는 SSL 인증서 문제 확인 필요)");
      } else {
        setError(err.message || "서버 연결 실패");
      }
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 2000);
    return () => clearInterval(interval);
  }, []);

  // 2. 혼잡도 레벨에 따른 UI 헬퍼 함수
  const getStatusInfo = (congestion: number, dist?: number) => {
    const distance = dist ?? 0; // 거리가 없으면 0 처리

    if (congestion === -1) {
      if (distance < 50) return { label: "테스트: 혼잡", img: "/high.png", color: "#ff4d4f" };
      if (distance < 150) return { label: "테스트: 보통", img: "/middle.png", color: "#faad14" };
      return { label: "테스트: 여유", img: "/low.png", color: "#52c41a" };
    }

    switch (congestion) {
      case 3: return { label: "매우 혼잡 (자리 없음)", img: "/high.png", color: "#cf1322" };
      case 2: return { label: "혼잡함", img: "/high.png", color: "#ff4d4f" };
      case 1: return { label: "보통", img: "/middle.png", color: "#faad14" };
      case 0: return { label: "여유로움", img: "/low.png", color: "#52c41a" };
      default: return { label: "상태 확인 불가", img: "/low.png", color: "#bfbfbf" };
    }
  };

  return (
    <div style={{ padding: '20px', textAlign: 'center', fontFamily: 'Arial, sans-serif' }}>
      <h1 style={{ color: '#333' }}>실시간 매점 혼잡도</h1>
      <hr style={{ margin: '20px 0', border: '0.5px solid #eee' }} />

      {error ? (
        <div style={{ marginTop: '50px', color: 'red' }}>
           <p style={{ fontSize: '18px', fontWeight: 'bold' }}>서버 연결 오류 발생</p>
           <p>{error}</p>
        </div>
      ) : data ? (
        <div style={{ transition: 'all 0.5s ease' }}>
          {/* 상태별 정보 추출 (distance가 없으면 d3 사용) */}
          {(() => {
            const displayDistance = data.distance ?? data.d3 ?? 0;
            const { label, img, color } = getStatusInfo(data.congestion, displayDistance);
            return (
              <>
                <div style={{ fontSize: '18px', color: '#666', marginBottom: '10px' }}>
                  평균 측정 거리: <strong>{displayDistance} cm</strong>
                </div>

                <img
                  src={img}
                  alt={label}
                  style={{
                    width: '300px',
                    height: '300px',
                    margin: '20px auto',
                    display: 'block',
                    borderRadius: '20px',
                    boxShadow: `0 10px 20px ${color}44`
                  }}
                />

                <p style={{
                  fontSize: '28px',
                  fontWeight: 'bold',
                  color: color,
                  padding: '10px',
                  borderRadius: '10px',
                  backgroundColor: `${color}11`
                }}>
                  {label}
                </p>
              </>
            );
          })()}
        </div>
      ) : (
        <div style={{ marginTop: '50px' }}>
          <p style={{ fontSize: '18px', color: '#999' }}>서버에 연결하여 데이터를 불러오는 중입니다...</p>
        </div>
      )}
    </div>
  );
};

export default DistanceMonitor;