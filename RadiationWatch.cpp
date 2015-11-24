/*
 * Radiation Watch Pocket Geiger Type 5 library for Arduino.
 *
 * Documentation and usage at:
 * https://github.com/MonsieurV/RadiationWatch
 *
 * Contributed by:
 * Radiation Watch <http://www.radiation-watch.org/>
 * thomasaw <https://github.com/thomasaw>
 * Tourmal <https://github.com/Toumal>
 * Yoan Tournade <yoan@ytotech.com>
 */
#include "RadiationWatch.h"

int volatile radiationCount = 0;
int volatile noiseCount = 0;
bool volatile radiationFlag = false;
bool volatile noiseFlag = false;
// Message buffer for output.
char _msg[256];

void _onRadiationHandler()
{
  radiationCount++;
  radiationFlag = true;
}

void _onNoiseHandler()
{
  noiseCount++;
  noiseFlag = true;
}

RadiationWatch::RadiationWatch(
  int signPin, int noisePin, int signIrq, int noiseIrq):
    _signPin(signPin), _noisePin(noisePin), _signIrq(signIrq),
    _noiseIrq(noiseIrq)
{
  previousTime = 0;
  _cpm = 0;
  cpmIndex = 0;
  cpmIndexPrev = 0;
  totalTime = 0;
  _radiationCallback = NULL;
  _noiseCallback = NULL;
}

void RadiationWatch::setup()
{
  pinMode(_signPin, INPUT);
  digitalWrite(_signPin, HIGH);
  pinMode(_noisePin, INPUT);
  digitalWrite(_noisePin, HIGH);
  // Initialize cpmHistory[].
  for(int i = 0; i < kHistoryCount; i++)
    _cpmHistory[i] = 0;
  // Init measurement time.
  previousTime = millis();
  // Attach interrupt handlers.
  attachInterrupt(_signIrq, _onRadiationHandler, RISING);
  attachInterrupt(_noiseIrq, _onNoiseHandler, RISING);
}

unsigned long loopTime = 0;
unsigned int loopElasped = 0;

void RadiationWatch::loop()
{
  // Process radiation dose if the process period has elapsed.
  loopElasped = loopElasped + abs(millis() - loopTime);
  loopTime = millis();

  // About 160-170 msec in Arduino Nano(ATmega328).
  // TODO To a constant.

  // TODO Update on radiation pulse, but not too fast (let a lag) so we does not
  // count radiation pulse when there are also noise. (use radiationFlag)
  if(loopElasped > 160) {
    // TODO Why it overflows? Serial.println(loopElasped);
    unsigned long currentTime = millis();
    if(noiseCount == 0) {
      // Shift an array for counting log for each 6 seconds.
      int totalTimeSec = (int) totalTime / 1000;
      if(totalTimeSec % 6 == 0 && cpmIndexPrev != totalTimeSec) {
        cpmIndexPrev = totalTimeSec;
        cpmIndex++;
        if(cpmIndex >= kHistoryCount)
          cpmIndex = 0;
        if(_cpmHistory[cpmIndex] > 0)
          _cpm -= _cpmHistory[cpmIndex];
        _cpmHistory[cpmIndex] = 0;
      }
      noInterrupts();
      // Store count log.
      _cpmHistory[cpmIndex] += radiationCount;
      // Add number of counts.
      _cpm += radiationCount;
      // Get the elapsed time.
      totalTime += abs(currentTime - previousTime);
      // TODO Maybe move? (reset even if there is noise)
      loopElasped = 0;
    }
    // Initialization for next N loops.
    previousTime = currentTime;
    radiationCount = 0;
    noiseCount = 0;
    interrupts();
    // Enable the callbacks.
    if(_radiationCallback && radiationFlag) {
      radiationFlag = false;
      _radiationCallback();
    }
    if(_noiseCallback && noiseFlag) {
      noiseFlag = false;
      _noiseCallback();
    }
  }
}

void RadiationWatch::registerRadiationCallback(void (*callback)(void))
{
  _radiationCallback = callback;
}

void RadiationWatch::registerNoiseCallback(void (*callback)(void))
{
  _noiseCallback = callback;
}

char* RadiationWatch::csvKeys()
{
  // CSV-formatting for output.
  return "time(ms),count,cpm,uSv/h,uSv/hError";
}

char* RadiationWatch::csvStatus()
{
  // Format message. We use dtostrf() to format float to string.
  char cpmBuff[10];
  char uSvBuff[10];
  char uSvdBuff[10];
  dtostrf(cpm(), -1, 3, cpmBuff);
  dtostrf(uSvh(), -1, 3, uSvBuff);
  dtostrf(uSvhError(), -1, 3, uSvdBuff);
  sprintf(_msg, "%lu,%d,%s,%s,%s",
    totalTime, radiationCount, cpmBuff, uSvBuff, uSvdBuff);
  return _msg;
}

unsigned long RadiationWatch::duration()
{
  return totalTime;
}

double RadiationWatch::cpm()
{
  double min = cpmTime();
  return (min > 0) ? _cpm / min : 0;
}

// cpm = uSv x alpha
static const double kAlpha = 53.032;

double RadiationWatch::uSvh()
{
  return cpm() / kAlpha;
}

double RadiationWatch::uSvhError()
{
  double min = cpmTime();
  return (min > 0) ? sqrt(_cpm) / min / kAlpha : 0;
}
