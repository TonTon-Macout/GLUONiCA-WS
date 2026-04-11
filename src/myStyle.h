#pragma once
#include <Arduino.h>
const char  mystyle[] PROGMEM = R"rawliteral(
    .weather-container {
      margin: 0 auto;
    }
    .header-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      /* padding: 0px 6px; */
      margin-bottom: 12px;
      font-size: 12px;
      padding-left: 8px;
    }
    .top-row {
      display: flex;
      justify-content: space-between;
    }
    .current-main {
      /* flex: 1; */
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
    }
    .weather-icon {
      font-size: 52px;
      margin-bottom: 10px;
    }
    .current-temp-big {
      /* flex: 1.35; */
      display: flex;
      flex-direction: column;
    }
    .temp-main {
      display: flex;
      align-items: center;
      flex: 1;
    }
    .big-temp {
      font-size: 42px;
      font-weight: bold;
      line-height: 1;
    }
    .day-night {
      font-size: 12px;
      line-height: 1.4;
      display: flex;
      justify-content: space-evenly;
    }
    .info-column {
      display: flex;
      flex-direction: column;
      align-items: flex-end;
      border-left: 2px solid var(--accent);
      padding-left: 12px;
    }
    .info-box {
      font-size: 12px;
    }
    .forecast-grid {
      display: grid;
      grid-template-columns: repeat(7, 1fr);
      justify-content: stretch;
      margin-top: 12px;
    }
    .forecast-day {
      text-align: center;
      display: flex;
      flex-direction: column;
      position: relative;
    }
    .forecast-icon {
      font-size: 22px;
      margin: 6px 0;
    }
    .forecast-day-name {
      font-size: 12px;
      opacity: 0.9;
    }
    .forecast-temp {
      font-size: 12px;
      font-weight: bold;
    }
/*  ====================== */
.inside-temp-container {
    display: flex;
    flex-wrap: nowrap;
    flex-direction: row;
    justify-content: flex-start;
    gap: 15px;
}
.extra-one {
    padding-bottom: 4px;
    border-bottom: 2px solid var(--accent);
}

.inside-extra_container {
    font-size: 16px;
    display: flex;
    flex-direction: column;
    justify-content: center;
    margin-right: 12px;
}

.extra-two {
    padding-top: 4px;
}
.inside-home {
    font-size: 48px;
    margin-top: -10px;
}


.inside-temp-big {
    font-size: 42px;
}
)rawliteral";