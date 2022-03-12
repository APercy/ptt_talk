g++ ptt_talk.cpp audio_rec_play.cpp libportaudio.a -lrt -lm -lasound -ljack -lopus -pthread -lsndfile -lX11 -o ptttalk
#gcc $1 libportaudio.a -lrt -lm -lasound -pthread -o $2
