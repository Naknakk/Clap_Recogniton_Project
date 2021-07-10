#if 1
#define LOG_OUT 1
#define FHT_N 256
#include <FHT.h>

/* Global Variables */
uint8_t amplitude[FHT_N * 2];
uint8_t avg[FHT_N / 2];
uint16_t statusBuffer; // Acts as Status Queue
int state;
int cnt;

/* Moving Average Funtion */
//   if n = 0 : avg_0 = fht_log_out
//   else :     avg_(n+1) = alpha * fht_log_out + (1 - alpha) * avg_(n)
//
//   Apply 'fht_log_out' to 'avg_(n+1)' only if :
//       lower limit < fht_log_out < upper limit
int count_upper_signals(){
    if(cnt < 3){
        cnt++;
        // simple average
        for(int i = 50; i < FHT_N/2; i++)
            avg[i] = avg[i] == 0 ? fht_log_out[i] : (fht_log_out[i] + avg[i] * (cnt - 1))/cnt; 
        return 0;
    }

    const float alpha = 0.8;
    const float lower = 0.5;
    const float upper = 1.5;

    int upper_signal = 0;

    // bar graph i = 40 at Freq = 1500 Hz
    // i = 50 at Freq = 1875 Hz 
    for(int i = 50; i < FHT_N/2; i++){
        float lower_limit = max(avg[i] - 20, lower * avg[i]);
        float upper_limit = min(avg[i] + 20, upper * avg[i]);

        // move average
        if(fht_log_out[i] > lower_limit && fht_log_out[i] < upper_limit){
            avg[i] = alpha * fht_log_out[i] + (1 - alpha) * avg[i];
        }
        // count up signals over than upper limit
        else if(fht_log_out[i] >= upper_limit){
            upper_signal++;
        }
    }

    return upper_signal;
}

/* Change the state between 0 and 1 */
void change_state(int upper_signal){
    const int level = 30; // level = 25 -> too low, level = 35 -> too much
    boolean statusOK = upper_signal > level; // check the signal if there are more 'upper signals' than the level

    statusBuffer |= statusOK; // add checked bit to LSB of buffer
    
    boolean isAcceptable = 
    (bitRead(statusBuffer, 6) | bitRead(statusBuffer, 5) | bitRead(statusBuffer, 4)) 
    & (!bitRead(statusBuffer, 3)) &
    (bitRead(statusBuffer, 2) | bitRead(statusBuffer, 1) | bitRead(statusBuffer, 0));

    // State Machine
    if(isAcceptable){
        if(state == 0){
            digitalWrite(LED_BUILTIN, HIGH);
            state = 1;
        }
        else{
            digitalWrite(LED_BUILTIN, LOW);
            state = 0;
        }
        statusBuffer = 0; // clear the buffer
        delay(50);
    }
    else{
        statusBuffer <<= 1; // shift the buffer
    }
}
#define REG 2
int REG = 2;
void setup(){
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    cli();

    TIMSK0 = 0;
    ADCSRA = (1 << ADEN) |
             (1 << ADSC) |
             (1 << ADATE) |
             (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    ADMUX = 0x40;
    DIDR0 = 0x01;
}

void loop(){
    
    for(int i = 0; i < FHT_N; i++){
        while(!(ADCSRA & (1 << ADIF)));

        uint8_t low = ADCL;//ADCL = 1111 0000
        uint8_t high = ADCH;

        amplitude[2*i] = high;
        amplitude[2*i + 1] = low;

        int toSigned = (high << 8 | low);
        toSigned -= 512;
        toSigned <<= 6;
        fht_input[i] = toSigned;

        ADCSRA |= (1 << ADSC);
    }

    fht_window();
    fht_reorder();
    fht_run();
    fht_mag_log();

    int num = count_upper_signals();
    change_state(num);

    Serial.write(255); // send a start byte
    Serial.write(amplitude, FHT_N * 2);
    Serial.write(fht_log_out, FHT_N/2);
}

#endif