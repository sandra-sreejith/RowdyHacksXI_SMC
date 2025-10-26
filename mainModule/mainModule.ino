int xPin = A0;
int yPin = A1;
int buttonPin = 9;

void setup() 
{

  Serial.begin(9600);
  pinMode(xPin, INPUT);
  pinMode(yPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);

}

void loop() 
{
  
  int xVal = analogRead(xPin);
  int yVal = analogRead(yPin);
  int button = digitalRead(buttonPin);

  Serial.print("Value of X: ");
  Serial.print(xVal);
  Serial.print(" | Value of Y: ");
  Serial.print(yVal);
  Serial.print(" | Button Pressed: ");
  Serial.print(button);
  Serial.print("\n");

}
