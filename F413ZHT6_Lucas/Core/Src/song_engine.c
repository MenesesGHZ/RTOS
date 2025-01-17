#include "song_engine.h"

#include "polyphonic_tunes.h"
#include "string.h"
#include "stdlib.h"

TIM_HandleTypeDef* tim_control;

volatile music current_song;
volatile song_status current_status;


uint8_t update_note (volatile music* musica, uint8_t instrument){

    char temp[5] = {'\0','\0','\0','\0','\0'};
    char bmol = 0;
    char sustenido = 0;
    char nota = 0;
    char pontuada = 0;
    char oitava[3]= {'\0','\0','\0'};

    const char* p = musica->voices[instrument];
    if(p[musica->control.posicao[instrument]] == '\0')  return 0;
    uint8_t aux1 = 0, aux2 = 0;

    while(p[musica->control.posicao[instrument]] != ',' && p[musica->control.posicao[instrument]] != '\0'){


        if(p[musica->control.posicao[instrument]] == 'a' || p[musica->control.posicao[instrument]] == 'b' || p[musica->control.posicao[instrument]] == 'c' || p[musica->control.posicao[instrument]] == 'd' || p[musica->control.posicao[instrument]] == 'e' || p[musica->control.posicao[instrument]] == 'f' || p[musica->control.posicao[instrument]] == 'g' || p[musica->control.posicao[instrument]] == 'p'  ){
            nota = p[musica->control.posicao[instrument]];
            musica->control.posicao[instrument]++;
        }

        else if(p[musica->control.posicao[instrument]] == '.'){
            pontuada = p[musica->control.posicao[instrument]];
            musica->control.posicao[instrument]++;
        }

        else if(p[musica->control.posicao[instrument]] == '_'){
            bmol = p[musica->control.posicao[instrument]];
            musica->control.posicao[instrument]++;
        }

        else if(p[musica->control.posicao[instrument]] == '#'){
            sustenido = p[musica->control.posicao[instrument]];
            musica->control.posicao[instrument]++;
        }

        else if(nota != 0){
            oitava[aux2] = p[musica->control.posicao[instrument]];
            aux2++;
            musica->control.posicao[instrument]++;
        }

        else{
            temp[aux1] = p[musica->control.posicao[instrument]];
            aux1 ++;
            musica->control.posicao[instrument]++;
        }
    }

    musica->control.duration[instrument] = (double) musica->nota_ref/atoi(temp);
    if (pontuada == '.') musica->control.duration[instrument] *= 3.0/2.0;

    switch (nota) {
		case 'a':
			musica->control.note[instrument] = 33;
			break;

		case 'b':
			musica->control.note[instrument] = 35;
			break;

		case 'c':
			musica->control.note[instrument] = 24;
			break;

		case 'd':
			musica->control.note[instrument] = 26;
			break;

		case 'e':
			musica->control.note[instrument] = 28;
			break;

		case 'f':
			musica->control.note[instrument] = 29;
			break;

        case 'g':
			musica->control.note[instrument] = 31;
			break;

        case 'p':
			musica->control.note[instrument] = 0;
			break;

    }

    if (musica->control.note[instrument] != 0) {
		if(bmol == '_')
			musica->control.note[instrument] += -1;
		if(sustenido == '#')
			musica->control.note[instrument] += +1;
		if(oitava[0] != '\0')
			musica->control.note[instrument] += 12*(atoi(oitava));
		musica->control.note[instrument] += 12*(musica->oitava-1);

    }


    if (p[musica->control.posicao[instrument]] == ','){
        musica->control.posicao[instrument]++;
    }
	return 1;
}


volatile uint32_t ticks_aux_channel_3;
volatile uint32_t ticks_aux_channel_4;

void voice_update(uint8_t voice) {
	uint8_t st;
	st = update_note(&current_song, voice);
	if (st) {
		if (current_song.control.duration[voice] == 4.0/16.0) {
			volatile int a;
			a = 2;
		}
		uint32_t ticks_aux;
		ticks_aux = 60.0/current_song.bpm*current_song.control.duration[voice] * FS;
		current_song.control.remaining_ticks[voice] = ticks_aux * NR_PERC/100;
		current_song.control.pause_ticks[voice] = ticks_aux * (100-NR_PERC)/100;

		if (current_song.control.note[voice] != 0)
			mTrigger(voice, current_song.control.note[voice]);
		else
			pause(voice);
	} else {
		current_status = STOPPED;
		synth_suspend();
	}
}

void song_scheduler(TIM_HandleTypeDef* htim) {
	if (htim != tim_control) return;
	if (current_status != PLAYING) return;


	for (int i = 0; i < 4; i++) {
		if (current_song.voices[i] == NULL) continue;
		if (current_song.control.remaining_ticks[i] > 0) {
			current_song.control.remaining_ticks[i]--;
		} else if (current_song.control.remaining_ticks[i] == 0) {
			pause(i);
			current_song.control.pause_ticks[i]--;
		} else {
			current_song.control.pause_ticks[i]--;
		}
		if (current_song.control.pause_ticks[i] == 0)
			voice_update(i);
	}

	audio_synthesis();
}


void initialize_song_engine(double timer_freq, TIM_HandleTypeDef* ctrl_tim) {

	setup_synth_engine(timer_freq, ctrl_tim);

	for(int i=0; i<4; i++) {
		setupVoice(i, SINE, 0, ENVELOPE0, 127, 64);
	}

	current_status = STOPPED;
	synth_suspend();
}

void load_song(music musica) {
	current_song = musica;
	memset((uint16_t*)&current_song.control.posicao, 0, sizeof(current_song.control.posicao)*4);

	for (uint8_t i = 0; i < 4; i++) {
		voice_update(i);
	}
}

void clear_song() {
	memset((music*)&current_song, 0, sizeof(music));
}

void set_pwm_output(TIM_HandleTypeDef* output_tim, uint8_t out_channel) {
	setup_synth_pwm_output_handler(output_tim, out_channel);
}

void set_custom_output_handler(void (*output_handler)(uint32_t)) {
	setup_synth_custom_output_handler(output_handler);
}

void play_song() {
	synth_resume();
	current_status = PLAYING;
}

void pause_song() {

	synth_suspend();
	current_status = PAUSED;

}

void stop_song() {
	synth_suspend();

	memset((uint16_t*)&current_song.control.posicao, 0, sizeof(current_song.control.posicao)*4);

	for (uint8_t i = 0; i < 4; i++) {
		voice_update(i);
	}

	current_status = STOPPED;

}




song_status get_song_status() {
	return current_status;
}


