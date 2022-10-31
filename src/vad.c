#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "vad.h"
#include "pav_analysis.h"

const float FRAME_TIME = 10.0F; /* in ms. */
const int N_INIT = 4;                   /*iteraciones iniciales sobre las que haremos medias de las features*/
const int UNDECIDED_V_FRAMES = 1;       /**/
const int UNDECIDED_S_FRAMES = 9;        /**/
const float LLINDAR_FRIC = 0.9;         /**/
const float ZCR_LOW = 1.4;    /**/
const float ZCR_HIGH = 1.4;   /**/
const float AMPLITUDE_OFFSET = 3.6;     /**/

/* 
 * As the output state is only ST_VOICE, ST_SILENCE, or ST_UNDEF,
 * only this labels are needed. You need to add all labels, in case
 * you want to print the internal state in string format
 */

const char *state_str[] = {
  "UNDEF", "S", "V", "INIT"
};

const char *state2str(VAD_STATE st) {
  return state_str[st];
}

/* Define a datatype with interesting features */
typedef struct {
  float zcr;
  float p;
  float am;
} Features;

/* 
 * TODO: Delete and use your own features!
 */

Features compute_features(const float *x, int N) {
  /*
   * Input: x[i] : i=0 .... N-1 
   * Ouput: computed features
   */
  /* 
   * DELETE and include a call to your own functions
   *
   * For the moment, compute random value between 0 and 1 
   */
  Features feat;
  feat.p = compute_power(x,N);
  feat.zcr = compute_zcr(x, N, N/(FRAME_TIME*1e-03));
  feat.am = compute_am(x, N);
  return feat;
}

/* 
 * TODO: Init the values of vad_data
 */

VAD_DATA * vad_open(float rate, float alfa1) {
  VAD_DATA *vad_data = malloc(sizeof(VAD_DATA));
  vad_data->state = ST_INIT;
  vad_data->sampling_rate = rate;
  vad_data->frame_length = rate * FRAME_TIME * 1e-3;
  if (alfa1 == 0)
    alfa1 = 2;
  vad_data->alfa1 = alfa1;
  vad_data->umbral = 0;
  vad_data->umbral_zcr_bajo = 0;
  vad_data->umbral_zcr_alto = 0;
  vad_data->last_state = ST_INIT;                     /*Nos indica el último estado V ó S*/
  vad_data->frame = 0;                                /*Indice del frame actual*/
  vad_data->last_defined_frame = 0;                   /*Indice del último frame con V ó S*/
  return vad_data;
}

VAD_STATE vad_close(VAD_DATA *vad_data) {
  /* 
   * TODO: decide what to do with the last undecided frames
   */
  VAD_STATE state = vad_data->last_state;

  free(vad_data);
  return state;
}

unsigned int vad_frame_size(VAD_DATA *vad_data) {
  return vad_data->frame_length;
}

/* 
 * TODO: Implement the Voice Activity Detection 
 * using a Finite State Automata
 */

VAD_STATE vad(VAD_DATA *vad_data, float *x) {

  /* 
   * TODO: You can change this, using your own features,
   * program finite state automaton, define conditions, etc.
   */

  Features f = compute_features(x, vad_data->frame_length);
  vad_data->last_feature = f.p; /* save feature, in case you want to show */
  
  bool voice_zcr_low = f.zcr < vad_data->umbral_zcr_bajo;
  bool voice_zcr_high = f.zcr > vad_data->umbral_zcr_alto;
  bool silence_zcr = (f.zcr > vad_data->umbral_zcr_bajo) && (f.zcr < vad_data->umbral_zcr_alto);

  switch (vad_data->state) {
  case ST_INIT:                                                             /*Durante N_INIT muestras iniciales se calculara los umbrales de la señal*/
    if(vad_data->frame < N_INIT) {
      vad_data->umbral = vad_data->umbral + f.p;
      vad_data->umbral_zcr_bajo = vad_data->umbral_zcr_bajo + f.zcr;
      vad_data->umbral_zcr_alto = vad_data->umbral_zcr_alto + f.zcr;
      vad_data->umbral_amplitud = vad_data->umbral_amplitud + f.am; 
    }else{
      vad_data->state = ST_SILENCE;
      vad_data->umbral = vad_data->umbral/N_INIT + vad_data->alfa1;
      vad_data->umbral_fric = vad_data->umbral + vad_data->alfa1/LLINDAR_FRIC;
      vad_data->umbral_zcr_bajo = vad_data->umbral_zcr_bajo / (N_INIT*ZCR_LOW);
      vad_data->umbral_zcr_alto = ZCR_HIGH * vad_data->umbral_zcr_alto/N_INIT;
      vad_data->umbral_amplitud = vad_data->umbral_amplitud*(AMPLITUDE_OFFSET/N_INIT);
      
    }
    break;

  case ST_SILENCE:
    if ( (f.p > vad_data->umbral_fric)  ||  (f.am > vad_data->umbral_amplitud)) {
      vad_data->state = ST_MAYBE_VOICE;
    }
      vad_data->last_state = ST_SILENCE;
      vad_data->last_defined_frame = vad_data->frame;

    break;

  case ST_VOICE:
    if ((f.p < vad_data->umbral_fric && f.am < vad_data->umbral_amplitud) && silence_zcr)
      vad_data->state = ST_MAYBE_SILENCE;
    vad_data->last_state = ST_VOICE;
    vad_data->last_defined_frame = vad_data->frame;
    
    break;

  case ST_MAYBE_SILENCE:
    if (((f.p > vad_data->umbral && voice_zcr_low) || (f.p < vad_data->umbral_fric && voice_zcr_high))  && (f.am > vad_data->umbral_amplitud)){
      vad_data->state = ST_VOICE;
    }
    else if((vad_data->frame - vad_data->last_defined_frame) == UNDECIDED_S_FRAMES){
      vad_data->state = ST_SILENCE;
    }
    break;
  
  case ST_MAYBE_VOICE:
    if ((f.p < vad_data->umbral_fric  && silence_zcr) || (f.am < vad_data->umbral_amplitud)){
      vad_data->state = ST_SILENCE;
    }
    else if((vad_data->frame - vad_data->last_defined_frame) == UNDECIDED_V_FRAMES){
      vad_data->state = ST_VOICE;
    }
    break;

  case ST_UNDEF:
    break;
  }

  vad_data->frame++;

  if (vad_data->state == ST_SILENCE ||
      vad_data->state == ST_VOICE)
    return vad_data->state;
  else if (vad_data->state == ST_INIT)
    return ST_SILENCE;
  else
    return ST_UNDEF;
}

void vad_show_state(const VAD_DATA *vad_data, FILE *out) {
  fprintf(out, "%d\t%f\n", vad_data->state, vad_data->last_feature);
}
