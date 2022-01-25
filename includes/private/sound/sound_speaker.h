#ifndef _SOUND_SPEAKER_H_
#define _SOUND_SPEAKER_H_
void speaker_init();

extern int speaker_mute;

extern int speaker_gated;
extern int speaker_enable, was_speaker_enable;

void speaker_update();


#endif /* _SOUND_SPEAKER_H_ */
