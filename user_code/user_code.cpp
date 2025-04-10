#include "user_code.h"
#include "bm_network.h"
#include "bm_printf.h"
#include "bm_pubsub.h"
#include "bristlefin.h"
#include "bsp.h"
#include "debug.h"
#include "lwip/inet.h"
#include "payload_uart.h"
#include "sensors.h"
#include "stm32_rtc.h"
#include "task_priorities.h"
#include "uptime.h"
#include "usart.h"
#include "util.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 250 // Changed to 10 seconds
#define DEFAULT_BAUD_RATE 57600
#define DEFAULT_LINE_TERM 13 // FL / '\n', 0x0A
#define BYTES_CLUSTER_MS 50

// app_main passes a handle to the user config partition in NVM.
extern cfg::Configuration *userConfigurationPartition;

// A timer variable we can set to trigger a pulse on LED2 when we get payload serial data
static int32_t ledLinePulse = -1;
static u_int32_t baud_rate_config = DEFAULT_BAUD_RATE;
static u_int32_t line_term_config = DEFAULT_LINE_TERM;
static bool waiting_for_response = false;
static uint32_t write_timer = -105000;

//iSAMI Abs Coef. constants for instruments made after June 2024; 
double ea434 = 19038;
double ea578 = 186;
double eb434 = 2553;
double eb578 = 41296;

// A buffer for our data from the payload uart
char payload_buffer[2048];

// structure for all temperature values, all units in [C]
typedef struct{
  double start_ex_temp;
  double end_ex_temp;
  double ex_temp; // average of start and end
  double in_temp;
} Temperature;

// structure for linear regression
typedef struct {
  double slope;
  double intercept; //pH_0
  double correlation;
  int size;
} RegressionResult;

// Structure to store aggregated statistics from one of our sensors.
typedef struct {
  float pH_final;
  float temp_final;
 } __attribute__((__packed__)) sensorStatAgg_t;

// structure to hold relavent pH variables
typedef struct {
  uint32_t time;
  double Salinity = 35;
  Temperature temperature;
  double A434_blank;
  double A578_blank;
  double A434_reagent[23];  //these values have their respective blank subtracted
  double A578_reagent[23];
  RegressionResult regression;  // nested linear regression struct for indicator purturb. results
  uint16_t battery;
  uint8_t checksum;
} pH_Record;

pH_Record record;
//void getHex(char arr[], int index[], int length[], int size){
  
//}

double steinhartHart(int voltage, double A, double B, double C, double D){
  double Rt1 = ((double)voltage / (16384 - voltage)) * 17400;	 // Rt1=17400 * temp/(maxVoltage-temp) # this in external temperature at start for SAMI, this equation is not valid for internal temperature (iSAMI0
  double InvT1 = A + B * (log(Rt1)) + C * pow(log(Rt1),2) + D * pow(log(Rt1),3);
  double TempK1 = 1 / InvT1;
  double TempC1 = TempK1 - 273.15;
  //TempC1 = TempC1 + tempOffset
  //TempFinal = TempC1
  //printf("%f\n", Rt1);      // prints: 3.141590
  //printf("%f\n", InvT1);
  //printf("%f\n", TempK1);
  //printf("%f\n\n", TempC1);
  return TempC1;
}

void calcTemperatures(char arr[]){
  int index[] = {16, 452, 460};
  int length[] = {4, 4, 4};

  int size = sizeof(index)/sizeof(index[0]);
  
  int bits[size];
  for(int i = 0; i < size; i++){
    
    char subsection[length[i]];  
    memcpy(subsection, &arr[index[i]], length[i]);
    subsection[length[i]] = '\0';
  
    bits[i] = (int)strtol(subsection, NULL, 16);
    //printf("[individual] | tempBits: %s\n", subsection);
  }

  record.temperature.start_ex_temp = steinhartHart(bits[0], 0.0010183, 0.000241, 0, 0.00000015);
  record.temperature.in_temp= steinhartHart(bits[1], 0.0010183, 0.000241, 0, 0.00000015);//0.0033540154, 0.00025627725, 0.0000020829210, 0.000000073003206);
  record.temperature.end_ex_temp = steinhartHart(bits[2], 0.0010183, 0.000241, 0, 0.00000015);

  record.temperature.ex_temp = (record.temperature.start_ex_temp + record.temperature.end_ex_temp) / 2;
}

// 32 4 sets of 'blank' light measurements (8 bytes ea.)
// 434 nm reference (dark adjusted) - 2 bytes
// 434 nm signal (dark adjusted) - 2 bytes
// 578 nm reference (dark adjusted) - 2 bytes
// 578 nm signal (dark adjusted) - 2 bytes
void blank(char arr[]) {
  // Extract subsection from input array
  char subsection[64];
  memcpy(subsection, &arr[20], 64);
  subsection[64] = '\0';
  
  double T434[4], T578[4]; //Fraction Transmitted (T)
  double r434 = 0, r578 = 0;
  
  // Process 4 sets of measurements
  for(int i = 0; i < 64; i += 16) {
      // Get 16-char set and convert to intensities
      int bits[4];
      for(int j = 0; j < 16; j += 4) {
          char intensity[5];
          memcpy(intensity, &subsection[i + j], 4);
          intensity[4] = '\0';
          bits[j/4] = (int)strtol(intensity, NULL, 16);
      }
      
      // Calculate ratios
      T434[i/16] = (double)bits[1]/bits[0]; //T = I/I_D
      T578[i/16] = (double)bits[3]/bits[2];
      r434 += T434[i/16];
      r578 += T578[i/16];
  }
  
  // Calculate averages and log values
  record.A434_blank = -log10(r434/4); //Abs = -log10(T)
  record.A578_blank = -log10(r578/4);
  
}


// 184 bytes (23 sets of 8 bytes)
void reagent(char arr[]){
  //subsection of reagent measurements
  char subsection[368];
  memcpy(subsection, &arr[84], 368);
  subsection[368] = '\0';
  
  //printf("[subsection] | reagent: %s\n", subsection);

  double ratio434[23], ratio578[23];

  // Process 4 sets of measurements
  for(int i = 0; i < 368; i += 16) {
    // Get 16-char set and convert to intensities
    int bits[4];
    for(int j = 0; j < 16; j += 4) {
        char intensity[5];
        memcpy(intensity, &subsection[i + j], 4);
        intensity[4] = '\0';
        bits[j/4] = (int)strtol(intensity, NULL, 16);
        //printf("[individual] | sig: %s, bits: %u\n", intensity, bits[j/4]);
    }
    //printf("[bits] | 0: %u, 1: %u, 2: %u, 3: %u, size: %u\n", bits[0], bits[1], bits[2], bits[3], sizeof(bits)/sizeof(bits[0]));
    // Calculate ratios
    ratio434[i/16] = (double)bits[1]/bits[0];
    ratio578[i/16] = (double)bits[3]/bits[2];
    //printf("[individual] | i: %u, ratio434: %f, ratio578: %f\n", i/16, ratio434[i/16], ratio578[i/16]);

    record.A434_reagent[i/16] = -log10(ratio434[i/16])-record.A434_blank;
    record.A578_reagent[i/16] = -log10(ratio578[i/16])-record.A578_blank;

  }
  //printf("size of ratio: %u\n\n", sizeof(ratio434)/sizeof(ratio434[0]));
}


RegressionResult calculateRegression(double x[], double y[], int n) {
  double sum_x = 0, sum_y = 0, sum_xy = 0;
  double sum_x2 = 0, sum_y2 = 0;
  
  for(int i = 0; i < n; i++) {
      sum_x += x[i];
      sum_y += y[i];
      sum_xy += x[i] * y[i];
      sum_x2 += x[i] * x[i];
      sum_y2 += y[i] * y[i];
  }
  
  RegressionResult result;
  double n_d = (double)n;
  
  // Calculate slope (m)
  result.slope = (n_d * sum_xy - sum_x * sum_y) / 
                (n_d * sum_x2 - sum_x * sum_x);
  
  // Calculate y-intercept (b)
  result.intercept = (sum_y - result.slope * sum_x) / n_d;
  
  // Calculate correlation coefficient (r)
  double numerator = (n_d * sum_xy - sum_x * sum_y);
  double denominator = sqrt((n_d * sum_x2 - sum_x * sum_x) * 
                          (n_d * sum_y2 - sum_y * sum_y));
  result.correlation = numerator / denominator;
  
  result.size = n;
  return result;
}

RegressionResult calculateRegressionWithBounds(double x[], double y[], int n, double lower_bound, double upper_bound) {
  // Temporary arrays to store filtered values
  double filtered_x[n];
  double filtered_y[n];
  int filtered_count = 0;
  
  // Filter values within bounds
  for(int i = 0; i < n; i++) {
      if(x[i] >= lower_bound && x[i] <= upper_bound) {
          filtered_x[filtered_count] = x[i];
          filtered_y[filtered_count] = y[i];
          filtered_count++;
      }
  }
  
  // Use filtered values for regression
  return calculateRegression(filtered_x, filtered_y, filtered_count);
}

void pH(char arr[]){

  // parse & calculate incoming temperature data
  calcTemperatures(arr);

  printf("[Temperature] | Start: %f, Int: %f, End: %f\n", record.temperature.start_ex_temp, record.temperature.in_temp, record.temperature.end_ex_temp);
  
  // parse & calc. incoming blank Abs. readings
  blank(arr);
  
  printf("[Blank Abs] | Blank 434: %f, Blank 578: %f\n", record.A434_blank, record.A578_blank);

  // parse & calc. incoming reagent Abs. readings
  reagent(arr);

  // // Absorptivity Coef. Calculations
  // acid dissociation constant
  double T_C = record.temperature.ex_temp;
  double T_K = T_C + 273.15;
  double pKa = -241.462 + 0.6375 + (7085.72 / T_K) + 43.8332 * log(T_K) - 0.0806406 * T_K - 0.3238 * pow(record.Salinity, 0.5) + 0.0807 * record.Salinity - 0.01157 * pow(record.Salinity, 1.5) + 0.000694 * pow(record.Salinity, 2); 
	

  // coef temperature compensation	
	double Ea434 = ea434 + 23.8680 * (25 - T_C);
	double Ea578 = ea578;
	double Eb434 = eb434 -	 8.7697 * (25 - T_C);
	double Eb578 = eb578 + 104.5411 * (25 - T_C);

  // Convenient Ratios
	double e1 = Ea578 / Ea434;
	double e2 = Eb578 / Ea434;
	double e3 = Eb434 / Ea434;

  // // Calculate indicator concentraition, Abs ratio & pH points
  double HI[23], I[23], Iconc[23], R[23], pH[23];
  int length = sizeof(record.A434_reagent)/sizeof(record.A434_reagent[0]);
  for(int i = 0;i < length;i++){
    // indicator concentraition
	  HI[i] = ((record.A578_reagent[i] * Eb578) - (record.A578_reagent[i] * Eb434)) / ((Ea434 * Eb578) - (Eb434 * Ea578));  
	  I[i] = ((record.A434_reagent[i] * Ea434) - (record.A434_reagent[i] * Ea578)) / ((Ea434 * Eb578) - (Eb434 * Ea578));
    Iconc[i] = HI[i] + I[i];

    //Abs. Ratio
    R[i] = record.A578_reagent[i]/record.A434_reagent[i];

    //pH points
    pH[i] = pKa + log10((R[i] - e1)/(e2 - R[i] * e3));

    if(Iconc[i] >= 0.00001 && Iconc[i] <= 0.00008){
      printf("    HI: %f, I: %f, Iconc: %f, R: %f, pH: %f\n", HI[i], I[i], Iconc[i], R[i], pH[i]);	 
    } else{
      printf("HI: %f, I: %f, Iconc: %f, R: %f, pH: %f\n", HI[i], I[i], Iconc[i], R[i], pH[i]);	 
    }
  }

  //pH = m*[HI]+b
  RegressionResult result = calculateRegressionWithBounds(Iconc, pH, 23, 0.00001, 0.00008); // Example bounds

  printf("Slope: %f\n", result.slope);
  printf("Y-intercept: %f\n", result.intercept);
  printf("Correlation coefficient: %f\n", result.correlation);
  printf("Valid points: %u\n", result.size);
    
  record.regression = result;


}


void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  // Retrieve user-set config values out of NVM.
  userConfigurationPartition->getConfig("plUartBaudRate", strlen("plUartBaudRate"),
                                        baud_rate_config);
  userConfigurationPartition->getConfig("plUartLineTerm", strlen("plUartLineTerm"),
                                        line_term_config);
  // Setup the UART – the on-board serial driver that talks to the RS232 transceiver.
  PLUART::init(USER_TASK_PRIORITY);
  // Baud set per expected baud rate of the sensor.
  PLUART::setBaud(baud_rate_config);
  // Enable passing raw bytes to user app.
  PLUART::setUseByteStreamBuffer(true);
  // Enable parsing lines and passing to user app.
  /// Warning: PLUART only stores a single line at a time. If your attached payload sends lines
  /// faster than the app reads them, they will be overwritten and data will be lost.
  PLUART::setUseLineBuffer(true);
  // Set a line termination character per protocol of the sensor.
  PLUART::setTerminationCharacter((char)line_term_config);
  // Turn on the UART.
  PLUART::enable();
  // Enable the input to the Vout power supply.
  bristlefin.enableVbus();
  // ensure Vbus stable before enable Vout with a 5ms delay.
  vTaskDelay(pdMS_TO_TICKS(5));
  // enable Vout, 12V by default.
  bristlefin.enableVout();
}

void loop(void) {
  /* USER LOOP CODE GOES HERE */
  static bool led2State = false;
  /// This checks for a trigger set by ledLinePulse when data is received from the payload UART.
  ///   Each time this happens, we pulse LED2 Green.
  // If LED2 is off and the ledLinePulse flag is set (done in our payload UART process line function), turn it on Green.
  if (!led2State && ledLinePulse > -1) {
    bristlefin.setLed(2, Bristlefin::LED_GREEN);
    led2State = true;
  }
  // If LED2 has been on for LED_ON_TIME_MS, turn it off.
  else if (led2State &&
           ((u_int32_t)uptimeGetMs() - ledLinePulse >= LED_ON_TIME_MS)) {
    bristlefin.setLed(2, Bristlefin::LED_OFF);
    ledLinePulse = -1;
    led2State = false;
  }

  /// This section demonstrates a simple non-blocking bare metal method for rollover-safe timed tasks,
  ///   like blinking an LED.
  /// More canonical (but more arcane) modern methods of implementing this kind functionality
  ///   would bee to use FreeRTOS tasks or hardware timer ISRs.
  static u_int32_t ledPulseTimer = uptimeGetMs();
  static u_int32_t ledOnTimer = 0;
  static bool led1State = false;
  // Turn LED1 on green every LED_PERIOD_MS milliseconds.
  if (!led1State &&
      ((u_int32_t)uptimeGetMs() - ledPulseTimer >= LED_PERIOD_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_GREEN);
    ledOnTimer = uptimeGetMs();
    ledPulseTimer += LED_PERIOD_MS;
    led1State = true;
  }
  // If LED1 has been on for LED_ON_TIME_MS milliseconds, turn it off.
  else if (led1State &&
           ((u_int32_t)uptimeGetMs() - ledOnTimer >= LED_ON_TIME_MS)) {
    bristlefin.setLed(1, Bristlefin::LED_OFF);
    led1State = false;
  }


  // // -- A timer is used to try to keep clusters of bytes (say from lines) in the same output.
  static int64_t readingBytesTimer = -1;

  // Read a line if it is available
  // Note - PLUART::setUseLineBuffer must be set true in setup to enable lines.
  if (PLUART::lineAvailable()) {
    // Shortcut the raw bytes cluster completion so the parsed line will be on a new console line
    if (readingBytesTimer > -1) {
      printf("\n");
      readingBytesTimer = -1;
    }
    uint16_t read_len = PLUART::readLine(payload_buffer, sizeof(payload_buffer));

    // Get the RTC if available
    RTCTimeAndDate_t time_and_date = {};
    rtcGet(&time_and_date);
    char rtcTimeBuffer[32];
    rtcPrint(rtcTimeBuffer, &time_and_date);

    if(read_len == 466){
      pH(payload_buffer);
      // Print the payload data to a file, to the bm_printf console, and to the printf console.
      bm_fprintf(0, "iSAMI_data.log", "tick: %llu, rtc: %s, pH: %f, T: %f\n",
        uptimeGetMs(), rtcTimeBuffer, record.regression.intercept, record.temperature.ex_temp);
      bm_printf(0, "[iSAMI] | tick: %llu, rtc: %s, pH: %f, T: %f", uptimeGetMs(),
        rtcTimeBuffer, record.regression.intercept, record.temperature.ex_temp);
      printf("[iSAMI] | tick: %llu, rtc: %s, pH: %f, T: %f\n", uptimeGetMs(),
        rtcTimeBuffer, record.regression.intercept, record.temperature.ex_temp);
      printf("Read length: %u\n", read_len);  

      // We have 2 sensor channels - humidity and temperature
      static const uint8_t NUM_SENSORS = 1;

      sensorStatAgg_t report_stats[NUM_SENSORS] = {};

      report_stats[0].pH_final = record.regression.intercept;
      report_stats[0].temp_final = record.temperature.ex_temp;

      uint8_t tx_data[sizeof(sensorStatAgg_t) * NUM_SENSORS] = {};
      for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        memcpy(tx_data + sizeof(sensorStatAgg_t) * i, reinterpret_cast<uint8_t *>(&report_stats[i]), sizeof(sensorStatAgg_t));
      }
      if(spotter_tx_data(tx_data, sizeof(sensorStatAgg_t) * NUM_SENSORS, BM_NETWORK_TYPE_CELLULAR_IRI_FALLBACK)){
        printf("%llut - %s | Successfully sent Spotter transmit data request\n", uptimeGetMs(), rtcTimeBuffer);
      }
      else {
        printf("%llut - %s | ERR Failed to send Spotter transmit data request!\n", uptimeGetMs(), rtcTimeBuffer);
      }
    }else{
      // Print the payload data to a file, to the bm_printf console, and to the printf console.
      bm_fprintf(0, "payload_data.log", "tick: %llu, rtc: %s, line: %.*s\n",
        uptimeGetMs(), rtcTimeBuffer, read_len, payload_buffer);
      bm_printf(0, "[payload] | tick: %llu, rtc: %s, line: %.*s", uptimeGetMs(),
        rtcTimeBuffer, read_len, payload_buffer);
      printf("[payload-line] | tick: %llu, rtc: %s, line: %.*s\n", uptimeGetMs(),
        rtcTimeBuffer, read_len, payload_buffer);
      printf("Read length: %u\n", read_len);  
    }

    ledLinePulse = uptimeGetMs(); // trigger a pulse on LED2
    waiting_for_response = false;

    
    
  }
 
  if (write_timer + 120000 < uptimeGetMs()) {
    waiting_for_response = true;
    const char command[] = "R 5A 0\r";
    //uint8_t data[] = {0x52, 0x20, 0x35, 0x41, 0x0D};
    printf("Writing: %s to the sensor!\n", command);
    PLUART::write((uint8_t *)command, strlen(command));
    printf("Done writing!\n");
    write_timer = uptimeGetMs();
  }

  if (write_timer + 9999999 < uptimeGetMs()) {
    char payload[2048]=":1A4E70AE41361D619EA157828870FC42A25158728810FE52A2A156A288C0FBC2A24156C287B0FBA2A20156824FB0FC6293B157C1A430FE5265715840F020FDB21DF157109FD0FCF1EF6155B090D0FC01E3B15690A600FBD1EFE15680CF70FB82081157C10400FC12215157E13C50FEA23BD156817540FBA251815781A820FC1264915771D0F0FB2270815621F780FD227C6157E21990FD42867158123380FBB28D5156C24750FCD292C156A25560FBC2961157825F60FBB29791587267A0FBD2993155226DB0FB529AB158627300FC729C8155727770FBD29D7157A27B20FBD29E419E13C7619CD8D";
    int read_len = sizeof(payload)/sizeof(payload[0]);
    pH(payload); 
    printf("[iSAMI] | tick: %llu, pH: %f, T: %f\n", uptimeGetMs(), record.regression.intercept, record.temperature.ex_temp);
    printf("Read length: %u\n", read_len);
  //   printf("[payload-line] | line: %s\n", payload);
    
  //   //int index_array[] = {8, 16, 20, 84, 452, 456, 460, 464};
  //   //int length_array[] = {8, 4, 64, 368, 4, 4, 4, 2};

  //   //int size_array = sizeof(index_array)/sizeof(index_array[0]);

  //   //getHex(payload,index_array,length_array,size_array);
  //   calcTemperatures(payload);

  //   printf("[Temperature] | Start: %f, Int: %f, End: %f\n", record.start_therm, record.int_therm, record.end_therm);
    
  //   blank(payload);
    
  //   printf("[Blank Abs] | Blank 434: %f, Blank 578: %f\n", record.A434_blank, record.A578_blank);

  //   reagent(payload);
    
  //   // int lengt=sizeof(record.A434_reagent)/sizeof(record.A434_reagent[0]);
  //   // for(int i = 0; i < lengt; i++){
  //   //   printf("[Reagent Abs] | 434: %f, 578: %f\n", record.A434_reagent[i], record.A578_reagent[i]);
  //   // }

  //   pH();

    write_timer = uptimeGetMs();
  }
}
