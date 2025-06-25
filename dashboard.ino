#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
// === WiFi Credentials ===
const char* ssid = "REDMI K50I";
const char* password = "123456789";
// === Web Server ===
ESP8266WebServer server(80);
// === Sensor Data ===
float temperature = 0.0;
float pHValue = 0.0;
float tdsValue = 0.0;
float turbidityValue = 0.0;
bool ipSent = false;








// Minute-wise data storage
const int MINUTES_TO_STORE = 60; // Store only 1 hour of minute data instead of 1440
struct MinuteData {
  float sum;
  int count;
  float average;
};








// Arrays to store minute-wise data
MinuteData tempMinutes[MINUTES_TO_STORE];
MinuteData phMinutes[MINUTES_TO_STORE];
MinuteData tdsMinutes[MINUTES_TO_STORE];
MinuteData turbMinutes[MINUTES_TO_STORE];
String minuteTimeLabels[MINUTES_TO_STORE];
int currentMinuteIndex = 0;
unsigned long lastMinute = 0;








// Data storage for historical values
const int MAX_DATA_POINTS = 60; // 1 hour of data (reduced from 288)
float tempHistory[MAX_DATA_POINTS];
float phHistory[MAX_DATA_POINTS];
float tdsHistory[MAX_DATA_POINTS];
float turbHistory[MAX_DATA_POINTS];
String timeLabels[MAX_DATA_POINTS];
int historyIndex = 0;
unsigned long lastStorageTime = 0;
const unsigned long STORAGE_INTERVAL = 60000; // 1 minute in milliseconds (reduced from 5 minutes)








// Functions to generate CSV data based on period
String generateTempCSV(String period) {
  String csv = "Time,Temperature (°C)\r\n";
 
  int numPoints;
  if (period == "min") {
    numPoints = 10; // Last 10 minutes
  } else if (period == "hour") {
    numPoints = min(60, currentMinuteIndex); // Up to last 60 minutes (1 hour)
  } else if (period == "day") {
    numPoints = min(60, currentMinuteIndex); // We only store 1 hour now
  } else {
    numPoints = 10; // Default to 10 minutes
  }
 
  // Start from the most recent complete minute
  int startIndex = (currentMinuteIndex - 1 + MINUTES_TO_STORE) % MINUTES_TO_STORE;
 
  for (int i = 0; i < numPoints; i++) {
    int idx = (startIndex - i + MINUTES_TO_STORE) % MINUTES_TO_STORE;
    if (tempMinutes[idx].count > 0) {
      csv += minuteTimeLabels[idx] + "," + String(tempMinutes[idx].average, 1) + "\r\n";
    }
  }
 
  return csv;
}








String generatePhCSV(String period) {
  String csv = "Time,pH\r\n";
 
  int numPoints;
  if (period == "min") {
    numPoints = 10; // Last 10 minutes
  } else if (period == "hour") {
    numPoints = min(60, currentMinuteIndex); // Up to last 60 minutes
  } else if (period == "day") {
    numPoints = min(60, currentMinuteIndex); // We only store 1 hour now
  } else {
    numPoints = 10; // Default to 10 minutes
  }
 
  // Start from the most recent complete minute
  int startIndex = (currentMinuteIndex - 1 + MINUTES_TO_STORE) % MINUTES_TO_STORE;
 
  for (int i = 0; i < numPoints; i++) {
    int idx = (startIndex - i + MINUTES_TO_STORE) % MINUTES_TO_STORE;
    if (phMinutes[idx].count > 0) {
      csv += minuteTimeLabels[idx] + "," + String(phMinutes[idx].average, 2) + "\r\n";
    }
  }
 
  return csv;
}








String generateTdsCSV(String period) {
  String csv = "Time,TDS (ppm)\r\n";
 
  int numPoints;
  if (period == "min") {
    numPoints = 10; // Last 10 minutes
  } else if (period == "hour") {
    numPoints = min(60, currentMinuteIndex); // Up to last 60 minutes
  } else if (period == "day") {
    numPoints = min(60, currentMinuteIndex); // We only store 1 hour now
  } else {
    numPoints = 10; // Default to 10 minutes
  }
 
  // Start from the most recent complete minute
  int startIndex = (currentMinuteIndex - 1 + MINUTES_TO_STORE) % MINUTES_TO_STORE;
 
  for (int i = 0; i < numPoints; i++) {
    int idx = (startIndex - i + MINUTES_TO_STORE) % MINUTES_TO_STORE;
    if (tdsMinutes[idx].count > 0) {
      csv += minuteTimeLabels[idx] + "," + String(tdsMinutes[idx].average, 0) + "\r\n";
    }
  }
 
  return csv;
}








String generateTurbidityCSV(String period) {
  String csv = "Time,Turbidity (NTU)\r\n";
 
  int numPoints;
  if (period == "min") {
    numPoints = 10; // Last 10 minutes
  } else if (period == "hour") {
    numPoints = min(60, currentMinuteIndex); // Up to last 60 minutes
  } else if (period == "day") {
    numPoints = min(60, currentMinuteIndex); // We only store 1 hour now
  } else {
    numPoints = 10; // Default to 10 minutes
  }
 
  // Start from the most recent complete minute
  int startIndex = (currentMinuteIndex - 1 + MINUTES_TO_STORE) % MINUTES_TO_STORE;
 
  for (int i = 0; i < numPoints; i++) {
    int idx = (startIndex - i + MINUTES_TO_STORE) % MINUTES_TO_STORE;
    if (turbMinutes[idx].count > 0) {
      csv += minuteTimeLabels[idx] + "," + String(turbMinutes[idx].average, 1) + "\r\n";
    }
  }
 
  return csv;
}








// === Dashboard HTML ===
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>Sensor Dashboard</title>
  <script src="https://cdn.jsdelivr.net/npm/canvas-gauges/gauge.min.js"></script>
  <style>
    body { font-family: Arial, sans-serif; background: #f4f7fc; margin: 0; padding: 0; }
    header { background-color: #4CAF50; color: white; padding: 20px; text-align: center; }
    .container {
      max-width: 1450px; margin: 20px auto; padding: 20px; background: white;
      box-shadow: 0 0 10px rgba(0,0,0,0.1); border-radius: 8px;
    }
    .gauges {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 20px;
      margin-top: 30px;
    }
    .gauge-block {
      text-align: center;
      background: #fff;
      border-radius: 10px;
      padding: 20px;
      box-shadow: 0 0 15px rgba(0, 0, 0, 0.1);
    }
    .gauge-value {
      font-size: 20px;
      font-weight: bold;
      margin-top: -10px;
    }
    .legend {
      font-size: 14px;
      margin-top: 5px;
    }
    canvas { display: block; margin: 20px auto; }
    .pH-labels, .TDS-labels, .temp-labels {
      display: flex;
      justify-content: space-between;
      margin-top: 10px;
      font-size: 12px;
    }
    .pH-labels span, .TDS-labels span, .temp-labels span {
      width: 18%;
      text-align: center;
      color: #333;
    }
    @media(max-width: 1200px) {
      .gauges {
        grid-template-columns: repeat(3, 1fr);
      }
    }
    @media(max-width: 900px) {
      .gauges {
        grid-template-columns: repeat(2, 1fr);
      }
    }
    @media(max-width: 600px) {
      .gauges {
        grid-template-columns: 1fr;
      }
    }
    .admin-btn { text-align: center; margin-top: 20px; }
    .admin-btn button {
      padding: 10px 20px; font-size: 16px; background-color: #1976d2;
      color: white; border: none; border-radius: 5px; cursor: pointer;
    }
    .alert {
  animation: blink 1s infinite;
  border: 3px solid red;
  box-shadow: 0 0 10px red;
}


@keyframes blink {
  0% { background-color: #fff; }
  50% { background-color: #ffcccc; }
  100% { background-color: #fff; }
}


  </style>
</head>
<body>
  <header><h1>🌡 Sensor DATA Monitoring </h1></header>
  <div class="container">
    <div class="gauges">
      <div class="gauge-block">
  <canvas id="gauge1"></canvas>
  <p id="temp" class="gauge-value">-- °C</p>
  <div class="temp-labels">
    <span style="color: #2196F3;">Cold</span>
    <span style="color: #4CAF50;">Cool</span>
    <span style="color: #FFEB3B;">Warm</span>
    <span style="color: #F44336;">Hot</span>
  </div>
   <div class="temp-history-link">
  <a href="/temp-history" target="_blank" title="View Temperature History">🔍 View History</a>
  </div>
</div>
      <div class="gauge-block">
        <canvas id="gaugePH"></canvas>
        <p id="ph" class="gauge-value">--</p>
        <div class="pH-labels">
          <span style="color: #f44336;">Strongly Acidic</span>
          <span style="color: #ff9800;">Weakly Acidic</span>
          <span style="color: #4CAF50;">Neutral</span>
          <span style="color: #2196F3;">Weakly Alkaline</span>
          <span style="color: #3f51b5;">Strongly Alkaline</span>
        </div>
      <div class="ph-history-link">
    <a href="/ph-history" target="_blank" title="View ph History">🔍 View History</a>
  </div>
  </div>
       <div class="gauge-block">
        <canvas id="gaugeTDS"></canvas>
        <p id="tds" class="gauge-value">-- ppm</p>
        <div class="TDS-labels">
          <span style="color: #4CAF50;">Low TDS</span>
          <span style="color: #FFEB3B;">Moderate TDS</span>
          <span style="color: #FF9800;">High TDS</span>
          <span style="color: #F44336;">Very High TDS</span>
        </div>
       <div class="tds-history-link">
     <a href="/tds-history" target="_blank" title="View TDS History">🔍 View History</a>
   </div>
   </div>
      <div class="gauge-block">
        <canvas id="gaugeTurbidity"></canvas>
        <p id="turb" class="gauge-value">-- NTU</p>
        <div class="legend">
          <span style="color: rgba(76, 175, 80, 1); font-weight: bold;">⬤ Clear</span> |
          <span style="color: rgba(255, 235, 59, 1); font-weight: bold;">⬤ Slightly Cloudy</span> |
          <span style="color: rgba(255, 152, 0, 1); font-weight: bold;">⬤ Cloudy</span> |
          <span style="color: rgba(244, 67, 54, 1); font-weight: bold;">⬤ Very Cloudy</span>
    </div>
       <div class="turb-history-link">
      <a href="/turb-history" target="_blank" title="View Turbidity History">🔍 View History</a>
   </div>
    </div>
     <div class="admin-btn">
      <button onclick="window.open('/login', '_blank', 'width=400,height=400')">🔐 Admin Login</button>
    </div>
    <div id="Water-Qulity" style="text-align:center; font-size: 24px; font-weight: bold; margin-top: 20px;">
  Water-Qulity: <span id="status-text">--</span>
</div>




  </div>
  <script>
    const gauge1 = new RadialGauge({
      renderTo: 'gauge1',
      width: 250,
      height: 250,
      units: "°C",
      title: "Temp",
      minValue: -10,
      maxValue: 100,
      majorTicks: ["-10","0","10","20","30","40","50","60","70","80","90","100"],
      minorTicks: 2,
      strokeTicks: true,
      highlights: [
         { from: -10, to: 0, color: "rgba(0, 200, 255, 0.5)" }, // Blue for Cold (from -10 to 0°C)
    { from: 0, to: 10, color: "rgba(0, 200, 0, 0.5)" },    // Green for Cold (from 0 to 10°C)
    { from: 10, to: 25, color: "rgba(255, 165, 0, 0.5)" },  // Orange for Cool (from 10 to 25°C)
    { from: 25, to: 35, color: "rgba(255, 235, 59, 0.5)" }, // Yellow for Warm (from 25 to 35°C)
    { from: 35, to: 100, color: "rgba(255, 0, 0, 0.75)" }  // Red for Hot (from 35 to 100°C)
  ],
      animation: true
    }).draw();
    const gaugePH = new RadialGauge({
      renderTo: 'gaugePH',
      width: 250,
      height: 250,
      units: "pH",
      title: "pH",
      minValue: 0,
      maxValue: 14,
      majorTicks: ["0", "2", "4", "6", "7", "8", "10", "12", "14"],
      minorTicks: 2,
      strokeTicks: true,
      highlights: [
        { from: 0, to: 6.0, color: "rgba(255, 0, 0, 0.75)" },      // Strongly Acidic
        { from: 6.0, to: 6.5, color: "rgba(255, 165, 0, 0.5)" },    // Weakly Acidic
        { from: 6.5, to: 7.0, color: "rgba(0, 200, 0, 0.5)" },     // Neutral
        { from: 7.1, to: 8.5, color: "rgba(0, 0, 255, 0.5)" },     // Weakly Alkaline
        { from: 8.6, to: 14.0, color: "rgba(0, 0, 139, 0.5)" }     // Strongly Alkaline
      ],
      value: 7,
      animation: true
    }).draw();
    const gaugeTDS = new RadialGauge({
      renderTo: 'gaugeTDS',
      width: 250,
      height: 250,
      units: "ppm",
      title: "TDS",
      minValue: 0,
      maxValue: 1000,
      majorTicks: ["0","100","200","300","400","500","600","700","800","900","1000"],
      minorTicks: 2,
      strokeTicks: true,
      highlights: [
        { from: 0, to: 300, color: "rgba(0, 200, 0, 0.5)" },     // Low TDS (Clean Water)
        { from: 300, to: 500, color: "rgba(255, 235, 59, 0.5)" },  // Moderate TDS
        { from: 500, to: 1000, color: "rgba(255, 152, 0, 0.5)" },  // High TDS
        { from: 1000, to: 2000, color: "rgba(244, 67, 54, 0.5)" }  // Very High TDS
      ],
      value: 0,
      animation: true
    }).draw();
    const gaugeTurbidity = new RadialGauge({
      renderTo: 'gaugeTurbidity',
      width: 250,
      height: 250,
      units: "NTU",
      title: "Turbidity",
      minValue: 0,
      maxValue: 100,
      majorTicks: ["0", "10", "20", "30", "40", "50", "60", "70", "80", "90", "100"],
      minorTicks: 2,
      strokeTicks: true,
      highlights: [
        { from: 0, to: 5, color: "rgba(76, 175, 80, 0.75)" },
        { from: 5, to: 10, color: "rgba(255, 235, 59, 0.75)" },
        { from: 10, to: 20, color: "rgba(255, 152, 0, 0.75)" },
        { from: 20, to: 100, color: "rgba(244, 67, 54, 0.75)" }
      ],
      colorPlate: "#fff",
      borderShadowWidth: 0,
      borders: false,
      needleType: "arrow",
      needleWidth: 3,
      needleCircleSize: 7,
      needleCircleOuter: true,
      needleCircleInner: false,
      animationDuration: 1500,
      animationRule: "linear",
      valueBox: true,
      value: 0
    }).draw();
    setInterval(() => {
      fetch('/data')
        .then(res => res.json())
        .then(data => {
          const temperature = data.temperature.toFixed(1);
          const ph = data.pH.toFixed(2);
          const tds = data.tds.toFixed(0);
          const turbidity = data.turbidity.toFixed(1);




          setInterval(() => {
  fetch('/data')
    .then(res => res.json())
    .then(data => {
      const temperature = data.temperature.toFixed(1);
      const ph = data.pH.toFixed(2);
      const tds = data.tds.toFixed(0);
      const turbidity = data.turbidity.toFixed(1);


      // Check quality conditions
      const tempOK = temperature <= 30;
      const phOK = ph >= 0 && ph <= 7.5;
      const tdsOK = tds >= 0 && tds <= 600;
      const turbOK = turbidity >= 0 && turbidity <= 5;


      const allGood = tempOK && phOK && tdsOK && turbOK;
      const statusText = document.getElementById('status-text');
      statusText.innerText = allGood ? "✅ Acceptable" : "❌ Poor";
      statusText.style.color = allGood ? "green" : "red";


      // Update Gauges
      gauge1.value = temperature;
      gaugePH.value = ph;
      gaugeTDS.value = tds;
      gaugeTurbidity.value = turbidity;


      // Update values
      document.getElementById('temp').innerText = ${temperature} °C;
      document.getElementById('ph').innerText = ${ph};
      document.getElementById('tds').innerText = ${tds} ppm;
      document.getElementById('turb').innerText = ${turbidity} NTU;


      // Apply or remove visual alert effect
      toggleAlert('gauge1', !tempOK);
      toggleAlert('gaugePH', !phOK);
      toggleAlert('gaugeTDS', !tdsOK);
      toggleAlert('gaugeTurbidity', !turbOK);
    });
}, 2000);


// Helper function to toggle alert effect
function toggleAlert(id, isAlert) {
  const element = document.getElementById(id).parentElement;
  if (isAlert) {
    element.classList.add('alert');
  } else {
    element.classList.remove('alert');
  }
}




















          gauge1.value = temperature;
          gaugePH.value = ph;
          gaugeTDS.value = tds;
          gaugeTurbidity.value = turbidity;








          document.getElementById('temp').innerText = ${temperature} °C;
          document.getElementById('ph').innerText = ${ph};
          document.getElementById('tds').innerText = ${tds} ppm;
          document.getElementById('turb').innerText = ${turbidity} NTU;
        });
    }, 2000);
  </script>
</body>
</html>
)rawliteral";
// === Temperature History Page ===
const char* tempHistoryPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Temperature History</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: Arial; background: #f0f0f0; text-align: center; padding: 30px; }
    canvas { background: white; border-radius: 10px; }
  </style>
</head>
<body>
  <h2>📈 Temperature History</h2>
  <a href="/download-temperature">
    <button style="margin-top: 20px; padding: 10px 20px; font-size: 16px;">&#x2B07;&#xFE0F; Download Excel (CSV)</button>
  </a>
  <label for="timePeriod">Select Time Period:</label>
  <select id="timePeriod" onchange="updateChart()">
    <option value="min">Last 10 Minutes</option>
    <option value="hour">Last 24 Hours</option>
    <option value="day">Last 7 Days</option>
  </select>
 <canvas id="tempChart" width="400" height="100"></canvas>
   <script>
    let tempData = []; // Array to hold historical data
    let tempLabels = []; // Time labels for historical data
    let selectedPeriod = "min"; // Default selected period
    const ctx = document.getElementById('tempChart').getContext('2d');
    const tempChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: tempLabels,
        datasets: [{
          label: 'Temperature (°C)',
          data: tempData,
          backgroundColor: 'rgba(33, 150, 243, 0.2)',
          borderColor: 'rgba(33, 150, 243, 1)',
          borderWidth: 2,
          fill: true
        }]
      },
      options: {
        responsive: true,
        scales: {
          x: {
            title: {
              display: true,
              text: 'Time'
            }
          },
          y: {
            title: {
              display: true,
              text: 'Temperature (°C)'
            }
          }
        }
      }
    });
    // Fetch data and update chart every 2 seconds
    setInterval(() => {
      fetch('/data')
        .then(res => res.json())
        .then(data => {
          // Update the data with new temperature
          const currentTime = new Date().toLocaleTimeString();
          const currentTemp = data.temperature;
          // Store the new temperature in the arrays
          tempData.push(currentTemp);
          tempLabels.push(currentTime);
          // Only keep the last 10 data points
          if (tempData.length > 10) {
            tempData.shift();
            tempLabels.shift();
          }
          // Update the chart
          tempChart.update();
        });
    }, 2000); // Update every 2 seconds
    function updateChart() {
      // Function to update chart based on selected time period
      selectedPeriod = document.getElementById('timePeriod').value;
           // This will change the label or data based on the selected period
      // You can add custom logic here to calculate min, hour, and day averages
      if (selectedPeriod === 'min') {
        // Show last 10 minutes (already implemented)
      } else if (selectedPeriod === 'hour') {
        // Implement logic for hourly averages
        alert("Hourly data is coming soon!");
      } else if (selectedPeriod === 'day') {
        // Implement logic for daily averages
        alert("Daily data is coming soon!");
      }
     
      // Update download URL with period parameter
      document.querySelector('a[href^="/download-temperature"]').href =
        /download-temperature?period=${selectedPeriod};
    }
   
    // Set initial download URL with default period
    document.querySelector('a[href^="/download-temperature"]').href =
      /download-temperature?period=${selectedPeriod};
  </script>
</body>
</html>
)rawliteral";








const char* phHistoryPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>pH History</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: Arial; background: #f0f0f0; text-align: center; padding: 30px; }
    canvas { background: white; border-radius: 10px; }
  </style>
</head>
<body>
  <h2>📈 pH History</h2>
  <a href="/download-ph">
  <button style="margin-top: 20px; padding: 10px 20px; font-size: 16px;">
    &#x2B07;&#xFE0F;Download Excel (CSV)
  </button>
</a>
  <label for="timePeriod">Select Time Period:</label>
  <select id="timePeriod" onchange="updateChart()">
    <option value="min">Last 10 Minutes</option>
    <option value="hour">Last 24 Hours</option>
    <option value="day">Last 7 Days</option>
  </select>
 <canvas id="phChart" width="400" height="100"></canvas>
   <script>
    let phData = []; // Array to hold historical pH data
    let phLabels = []; // Time labels for pH data
    let selectedPeriod = "min"; // Default selected period
    const ctx = document.getElementById('phChart').getContext('2d');
    const phChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: phLabels,
        datasets: [{
          label: 'pH',
          data: phData,
          backgroundColor: 'rgba(33, 150, 243, 0.2)',
          borderColor: 'rgba(33, 150, 243, 1)',
          borderWidth: 2,
          fill: true
        }]
      },
      options: {
        responsive: true,
        scales: {
          x: {
            title: {
              display: true,
              text: 'Time'
            }
          },
          y: {
            title: {
              display: true,
              text: 'pH'
            }
          }
        }
      }
    });
    // Fetch data and update chart every 2 seconds
    setInterval(() => {
      fetch('/data')
        .then(res => res.json())
        .then(data => {
          // Update the data with new pH
          const currentTime = new Date().toLocaleTimeString();
          const currentPH = data.pH;
          // Store the new pH in the arrays
          phData.push(currentPH);
          phLabels.push(currentTime);
          // Only keep the last 10 data points
          if (phData.length > 10) {
            phData.shift();
            phLabels.shift();
          }
          // Update the chart
          phChart.update();
        });
    }, 2000); // Update every 2 seconds
    function updateChart() {
      selectedPeriod = document.getElementById('timePeriod').value;
      if (selectedPeriod === 'min') {
        // Show last 10 minutes (already implemented)
      } else if (selectedPeriod === 'hour') {
        alert("Hourly data is coming soon!");
      } else if (selectedPeriod === 'day') {
        alert("Daily data is coming soon!");
      }
     
      // Update download URL with period parameter
      document.querySelector('a[href^="/download-ph"]').href =
        /download-ph?period=${selectedPeriod};
    }
   
    // Set initial download URL with default period
    document.querySelector('a[href^="/download-ph"]').href =
      /download-ph?period=${selectedPeriod};
  </script>
</body>
</html>
)rawliteral";
// === TDS History Page ===
const char* tdsHistoryPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>TDS History</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: Arial; background: #f0f0f0; text-align: center; padding: 30px; }
    canvas { background: white; border-radius: 10px; }
  </style>
</head>
<body>
  <h2>TDS History</h2>
  <a href="/download-tds">
  <button style="margin-top: 20px; padding: 10px 20px; font-size: 16px;">
      &#x2B07;&#xFE0F;Download Excel (CSV)
    </button>
  </a>
  <label for="timePeriod">Select Time Period:</label>
  <select id="timePeriod" onchange="updateChart()">
    <option value="min">Last 10 Minutes</option>
    <option value="hour">Last 24 Hours</option>
    <option value="day">Last 7 Days</option>
  </select>
<canvas id="tdsChart" width="400" height="100"></canvas>
   <script>
    let tdsData = []; // Array to hold historical data
    let tdsLabels = []; // Time labels for historical data
    let selectedPeriod = "min"; // Default selected period
   const ctx = document.getElementById('tdsChart').getContext('2d');
    const tdsChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: tdsLabels,
        datasets: [{
          label: 'TDS (ppm)',
          data: tdsData,
          backgroundColor: 'rgba(255, 152, 0, 0.2)',
          borderColor: 'rgba(255, 152, 0, 1)',
          borderWidth: 2,
          fill: true
        }]
      },
      options: {
        responsive: true,
        scales: {
          x: {
            title: {
              display: true,
              text: 'Time'
            }
          },
          y: {
            title: {
              display: true,
              text: 'TDS (ppm)'
            }
          }
        }
      }
    });
    // Fetch data and update chart every 2 seconds
    setInterval(() => {
      fetch('/data')
        .then(res => res.json())
        .then(data => {
          // Update the data with new TDS
          const currentTime = new Date().toLocaleTimeString();
          const currentTDS = data.tds;
          // Store the new TDS in the arrays
          tdsData.push(currentTDS);
          tdsLabels.push(currentTime);
          // Only keep the last 10 data points
          if (tdsData.length > 10) {
            tdsData.shift();
            tdsLabels.shift();
          }
          // Update the chart
          tdsChart.update();
        });
    }, 2000); // Update every 2seconds
    function updateChart() {
      // Function to update chart based on selected time period
      selectedPeriod = document.getElementById('timePeriod').value;
           // This will change the label or data based on the selected period
      // You can add custom logic here to calculate min, hour, and day averages
      if (selectedPeriod === 'min') {
        // Show last 10 minutes (already implemented)
      } else if (selectedPeriod === 'hour') {
        // Implement logic for hourly averages
        alert("Hourly data is coming soon!");
      } else if (selectedPeriod === 'day') {
        // Implement logic for daily averages
        alert("Daily data is coming soon!");
      }
     
      // Update download URL with period parameter
      document.querySelector('a[href^="/download-tds"]').href =
        /download-tds?period=${selectedPeriod};
    }
   
    // Set initial download URL with default period
    document.querySelector('a[href^="/download-tds"]').href =
      /download-tds?period=${selectedPeriod};
  </script>
</body>
</html>
)rawliteral";
// === Turbidity History Page ===
const char* turbidityHistoryPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Turbidity History</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: Arial; background: #f0f0f0; text-align: center; padding: 30px; }
    canvas { background: white; border-radius: 10px; }
  </style>
</head>
<body>
  <h2>🌫 Turbidity History</h2>
 <a href="/download-turbidity">
  <button style="margin-top: 20px; padding: 10px 20px; font-size: 16px;">
    &#x2B07;&#xFE0F; Download Excel (CSV)
  </button>
</a>
  <label for="timePeriod">Select Time Period:</label>
  <select id="timePeriod" onchange="updateChart()">
    <option value="min">Last 10 Minutes</option>
    <option value="hour">Last 24 Hours</option>
    <option value="day">Last 7 Days</option>
  </select>
  <canvas id="turbChart" width="400" height="100"></canvas>
   <script>
    let turbData = [];
    let turbLabels = [];
    let selectedPeriod = "min"; // Default selected period
   const ctx = document.getElementById('turbChart').getContext('2d');
    const turbChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: turbLabels,
        datasets: [{
          label: 'Turbidity (NTU)',
          data: turbData,
          backgroundColor: 'rgba(244, 67, 54, 0.2)',
          borderColor: 'rgba(244, 67, 54, 1)',
          borderWidth: 2,
          fill: true
        }]
      },
      options: {
        responsive: true,
        scales: {
          x: {
            title: {
              display: true,
              text: 'Time'
            }
          },
          y: {
            title: {
              display: true,
              text: 'Turbidity (NTU)'
            }
          }
        }
      }
    });
    setInterval(() => {
      fetch('/data')
        .then(res => res.json())
        .then(data => {
          const currentTime = new Date().toLocaleTimeString();
          const currentTurbidity = data.turbidity;
          turbData.push(currentTurbidity);
          turbLabels.push(currentTime);
          if (turbData.length > 10) {
            turbData.shift();
            turbLabels.shift();
          }
          turbChart.update();
        });
    }, 2000); // every 2 sec
    function updateChart() {
      selectedPeriod = document.getElementById('timePeriod').value;
      if (selectedPeriod === 'min') {
        // Show last 10 minutes (already implemented)
      } else if (selectedPeriod === 'hour') {
        // Implement logic for hourly averages
        alert("Hourly data is coming soon!");
      } else if (selectedPeriod === 'day') {
        // Implement logic for daily averages
        alert("Daily data is coming soon!");
      }
     
      // Update download URL with period parameter
      document.querySelector('a[href^="/download-turbidity"]').href =
        /download-turbidity?period=${selectedPeriod};
    }
   
    // Set initial download URL with default period
    document.querySelector('a[href^="/download-turbidity"]').href =
      /download-turbidity?period=${selectedPeriod};
  </script>
</body>
</html>
)rawliteral";
// === Login Page ===
const char* loginPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Admin Login</title>
<style>
  body { font-family: Arial; background: #f4f4f4; text-align: center; padding-top: 50px; }
  form { background: white; padding: 20px; border-radius: 8px;
         display: inline-block; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
  input { padding: 10px; margin: 10px; width: 80%; }
</style>
</head>
<body>
  <h2>Admin Login</h2>
  <form action="/auth" method="POST">
    <input type="text" name="username" placeholder="Username" required/><br/>
    <input type="password" name="password" placeholder="Password" required/><br/>
    <input type="submit" value="Login" />
  </form>
</body>
</html>
)rawliteral";
// === Admin Page ===
const char* adminPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Sensor Ranges</title>
  <style>
    body { font-family: Arial; background: #f9f9f9; padding: 20px; }
    form, #results, .back { max-width: 500px; margin: 20px auto; background: #fff; padding: 20px; border-radius: 10px; box-shadow: 0 0 8px #ccc; }
    h2, h3 { text-align: center; }
    label { font-weight: bold; display: block; margin-top: 10px; }
    .range { display: flex; gap: 10px; margin-bottom: 10px; }
    .range input { flex: 1; padding: 8px; }
    button { width: 100%; padding: 10px; margin-top: 10px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; }
    .save { background: #4CAF50; color: white; }
    .back button { background: #2196F3; color: white; }
  </style>
</head>
<body>




  <form id="sensorForm">
    <h2>📏 Set Sensor Ranges</h2>




    <label>Temperature (°C):</label>
    <div class="range"><input type="number" id="tempMin" placeholder="Min"><input type="number" id="tempMax" placeholder="Max"></div>




    <label>pH:</label>
    <div class="range"><input type="number" step="0.1" id="phMin"><input type="number" step="0.1" id="phMax"></div>




    <label>TDS (ppm):</label>
    <div class="range"><input type="number" id="tdsMin"><input type="number" id="tdsMax"></div>




    <label>Turbidity (NTU):</label>
    <div class="range"><input type="number" step="0.1" id="turbMin"><input type="number" step="0.1" id="turbMax"></div>




    <button type="submit" class="save">💾 Save Settings</button>
  </form>




  <div id="results" style="display:none;">
    <h3>🔍 Entered Ranges</h3>
    <p><b>Temperature:</b> <span id="showTemp"></span></p>
    <p><b>pH:</b> <span id="showPh"></span></p>
    <p><b>TDS:</b> <span id="showTds"></span></p>
    <p><b>Turbidity:</b> <span id="showTurb"></span></p>
  </div>




  <div class="back"><button onclick="location.href='/dashboard'">🏠 Back to Dashboard</button></div>




  <script>
    document.getElementById('sensorForm').onsubmit = e => {
      e.preventDefault();
      const get = id => document.getElementById(id).value;
      document.getElementById('showTemp').textContent = ${get('tempMin')} - ${get('tempMax')} °C;
      document.getElementById('showPh').textContent = ${get('phMin')} - ${get('phMax')};
      document.getElementById('showTds').textContent = ${get('tdsMin')} - ${get('tdsMax')} ppm;
      document.getElementById('showTurb').textContent = ${get('turbMin')} - ${get('turbMax')} NTU;
      document.getElementById('results').style.display = 'block';
    };
  </script>




</body>
</html>




)rawliteral";
// === Setup and Server Handling ===
void setup() {
  Serial.begin(115200);
 
  // Initialize minute data arrays
  for (int i = 0; i < MINUTES_TO_STORE; i++) {
    tempMinutes[i] = {0, 0, 0};
    phMinutes[i] = {0, 0, 0};
    tdsMinutes[i] = {0, 0, 0};
    turbMinutes[i] = {0, 0, 0};
    minuteTimeLabels[i] = "";
  }
 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.println("IP: " + WiFi.localIP().toString());
  server.on("/", []() { server.send(200, "text/html", htmlPage); });
  server.on("/data", []() {
    String json = "{";
    json += "\"temperature\":" + String(temperature, 1) + ",";
    json += "\"pH\":" + String(pHValue, 2) + ",";
    json += "\"tds\":" + String(tdsValue, 0) + ",";
    json += "\"turbidity\":" + String(turbidityValue, 1);
    json += "}";
    server.send(200, "application/json", json);
  });
  server.on("/login", []() { server.send(200, "text/html", loginPage); });  // Correct usage of loginPage here
  server.on("/auth", HTTP_POST, []() {
    if (server.arg("username") == "Admin" && server.arg("password") == "1234") {
      server.sendHeader("Location", "/admin", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(403, "text/html", "<h1>Login Failed</h1>");
    }
  });
  server.on("/admin", []() { server.send(200, "text/html", adminPage); });
  server.begin();
  server.on("/temp-history", []() {
    server.send(200, "text/html", tempHistoryPage);
  });
 
  // Add download handlers for CSV exports
  server.on("/download-temperature", []() {
    String period = server.arg("period");
    String csv = generateTempCSV(period);
    server.sendHeader("Content-Disposition", "attachment; filename=temperature_data.csv");
    server.sendHeader("Content-Type", "text/csv");
    server.send(200, "text/csv", csv);
  });








  server.on("/download-ph", []() {
    String period = server.arg("period");
    String csv = generatePhCSV(period);
    server.sendHeader("Content-Disposition", "attachment; filename=ph_data.csv");
    server.sendHeader("Content-Type", "text/csv");
    server.send(200, "text/csv", csv);
  });








  server.on("/download-tds", []() {
    String period = server.arg("period");
    String csv = generateTdsCSV(period);
    server.sendHeader("Content-Disposition", "attachment; filename=tds_data.csv");
    server.sendHeader("Content-Type", "text/csv");
    server.send(200, "text/csv", csv);
  });








  server.on("/download-turbidity", []() {
    String period = server.arg("period");
    String csv = generateTurbidityCSV(period);
    server.sendHeader("Content-Disposition", "attachment; filename=turbidity_data.csv");
    server.sendHeader("Content-Type", "text/csv");
    server.send(200, "text/csv", csv);
  });








  server.on("/ph-history", []() {
    server.send(200, "text/html", phHistoryPage);
  });








  server.on("/tds-history", []() {
    server.send(200, "text/html", tdsHistoryPage);
  });








  server.on("/turb-history", []() {
    server.send(200, "text/html", turbidityHistoryPage);
  });








}








void loop() {
  server.handleClient();
  // === Read Sensor Data from Arduino ===
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    int firstComma = data.indexOf(',');
    int secondComma = data.indexOf(',', firstComma + 1);
    int thirdComma = data.indexOf(',', secondComma + 1);
    if (firstComma > 0 && secondComma > firstComma && thirdComma > secondComma) {
      temperature = data.substring(0, firstComma).toFloat();
      pHValue = data.substring(firstComma + 1, secondComma).toFloat();
      tdsValue = data.substring(secondComma + 1, thirdComma).toFloat();
      turbidityValue = data.substring(thirdComma + 1).toFloat();
     
      // Get current minute
      unsigned long currentTime = millis();
      unsigned long currentMinute = currentTime / 60000; // Convert to minutes
     
      // If we're in a new minute, finalize the previous minute's averages and move to next slot
      if (currentMinute > lastMinute) {
        if (tempMinutes[currentMinuteIndex].count > 0) {
          tempMinutes[currentMinuteIndex].average = tempMinutes[currentMinuteIndex].sum / tempMinutes[currentMinuteIndex].count;
          phMinutes[currentMinuteIndex].average = phMinutes[currentMinuteIndex].sum / phMinutes[currentMinuteIndex].count;
          tdsMinutes[currentMinuteIndex].average = tdsMinutes[currentMinuteIndex].sum / tdsMinutes[currentMinuteIndex].count;
          turbMinutes[currentMinuteIndex].average = turbMinutes[currentMinuteIndex].sum / turbMinutes[currentMinuteIndex].count;
        }
       
        // Move to next minute slot
        currentMinuteIndex = (currentMinuteIndex + 1) % MINUTES_TO_STORE;
       
        // Reset the new slot
        tempMinutes[currentMinuteIndex] = {0, 0, 0};
        phMinutes[currentMinuteIndex] = {0, 0, 0};
        tdsMinutes[currentMinuteIndex] = {0, 0, 0};
        turbMinutes[currentMinuteIndex] = {0, 0, 0};
       
        // Store time label
        minuteTimeLabels[currentMinuteIndex] = String(currentMinute);
       
        lastMinute = currentMinute;
      }
      // Add current readings to the current minute's sums
      tempMinutes[currentMinuteIndex].sum += temperature;
      tempMinutes[currentMinuteIndex].count++;
     
      phMinutes[currentMinuteIndex].sum += pHValue;
      phMinutes[currentMinuteIndex].count++;
     
      tdsMinutes[currentMinuteIndex].sum += tdsValue;
      tdsMinutes[currentMinuteIndex].count++;
     
      turbMinutes[currentMinuteIndex].sum += turbidityValue;
      turbMinutes[currentMinuteIndex].count++;
    }
  }
}