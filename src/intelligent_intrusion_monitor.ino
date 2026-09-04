// ============================================================
// Intelligent Intrusion & Perimeter Monitor
// Version 5.0
// Sensor Health Monitoring + Fault Handling
// ============================================================

const byte TRIG_PIN = 9;
const byte ECHO_PIN = 10;

const byte LED_PIN = 6;
const byte BUZZER_PIN = 7;

const int SAMPLE_COUNT = 5;
float distanceSamples[SAMPLE_COUNT];
int sampleIndex = 0;
bool filterReady = false;

const float WARNING_THRESHOLD = 10.0;
const float INTRUSION_THRESHOLD = 5.0;
const int REQUIRED_INTRUSION_SAMPLES = 3;
int intrusionCounter = 0;

const float MIN_VALID_DISTANCE = 2.0;
const float MAX_VALID_DISTANCE = 400.0;
const int MAX_CONSECUTIVE_ERRORS = 3;
int consecutiveSensorErrors = 0;
bool sensorHealthy = true;

const unsigned long SENSOR_INTERVAL = 200;
const unsigned long FAULT_BLINK_INTERVAL = 250;
unsigned long lastSensorRead = 0;
unsigned long lastFaultBlink = 0;
bool faultOutputState = false;

enum SystemState
{
  NORMAL,
  WARNING,
  INTRUSION,
  SENSOR_FAULT
};

SystemState currentState = NORMAL;

float readRawDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0)
  {
    return -1.0;
  }

  return (duration * 0.0343) / 2.0;
}

bool isValidDistance(float distance)
{
  if (distance < MIN_VALID_DISTANCE)
  {
    return false;
  }

  if (distance > MAX_VALID_DISTANCE)
  {
    return false;
  }

  return true;
}

float calculateAverage()
{
  float sum = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++)
  {
    sum += distanceSamples[i];
  }

  return sum / SAMPLE_COUNT;
}

void addSample(float newDistance)
{
  distanceSamples[sampleIndex] = newDistance;

  sampleIndex++;

  if (sampleIndex >= SAMPLE_COUNT)
  {
    sampleIndex = 0;
    filterReady = true;
  }
}

const char* getStateName(SystemState state)
{
  switch (state)
  {
    case NORMAL:
      return "NORMAL";

    case WARNING:
      return "WARNING";

    case INTRUSION:
      return "INTRUSION";

    case SENSOR_FAULT:
      return "SENSOR_FAULT";

    default:
      return "UNKNOWN";
  }
}

void updateState(float filteredDistance)
{
  if (filteredDistance <= INTRUSION_THRESHOLD)
  {
    intrusionCounter++;

    if (intrusionCounter >= REQUIRED_INTRUSION_SAMPLES)
    {
      currentState = INTRUSION;
    }

    return;
  }

  intrusionCounter = 0;

  if (filteredDistance <= WARNING_THRESHOLD)
  {
    currentState = WARNING;
  }
  else
  {
    currentState = NORMAL;
  }
}

void updateNormalOutputs()
{
  switch (currentState)
  {
    case NORMAL:
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case WARNING:
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      break;

    case INTRUSION:
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
      break;

    default:
      break;
  }
}

void updateFaultOutputs(unsigned long currentTime)
{
  if (currentTime - lastFaultBlink >= FAULT_BLINK_INTERVAL)
  {
    lastFaultBlink = currentTime;
    faultOutputState = !faultOutputState;

    digitalWrite(LED_PIN, faultOutputState);
    digitalWrite(BUZZER_PIN, faultOutputState);
  }
}

void setup()
{
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.begin(9600);

  Serial.println("======================================");
  Serial.println(" Intelligent Intrusion Monitor V5");
  Serial.println(" Sensor Health + Fault Handling");
  Serial.println("======================================");
}

void loop()
{
  unsigned long currentTime = millis();

  if (currentState == SENSOR_FAULT)
  {
    updateFaultOutputs(currentTime);
  }

  if (currentTime - lastSensorRead >= SENSOR_INTERVAL)
  {
    lastSensorRead = currentTime;

    float rawDistance = readRawDistance();

    if (!isValidDistance(rawDistance))
    {
      consecutiveSensorErrors++;

      Serial.print("Sensor Error Count: ");
      Serial.println(consecutiveSensorErrors);

      if (consecutiveSensorErrors >= MAX_CONSECUTIVE_ERRORS)
      {
        sensorHealthy = false;
        currentState = SENSOR_FAULT;
        intrusionCounter = 0;

        Serial.println("!!! SENSOR FAULT !!!");
      }

      return;
    }

    if (!sensorHealthy)
    {
      Serial.println("Sensor recovered.");

      sensorHealthy = true;
      currentState = NORMAL;
      faultOutputState = false;

      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
    }

    consecutiveSensorErrors = 0;

    addSample(rawDistance);

    if (!filterReady)
    {
      Serial.print("Collecting samples: ");
      Serial.print(sampleIndex);
      Serial.print("/");
      Serial.println(SAMPLE_COUNT);
      return;
    }

    float filteredDistance = calculateAverage();

    updateState(filteredDistance);
    updateNormalOutputs();

    Serial.print("Raw: ");
    Serial.print(rawDistance);
    Serial.print(" cm | Filtered: ");
    Serial.print(filteredDistance);
    Serial.print(" cm | Intrusion Count: ");
    Serial.print(intrusionCounter);
    Serial.print(" | Sensor: ");
    Serial.print(sensorHealthy ? "HEALTHY" : "FAULT");
    Serial.print(" | State: ");
    Serial.println(getStateName(currentState));
  }
}
