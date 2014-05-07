all: reliable_sender reliable_receiver
reliable_sender: sender_main.c
		gcc $(CONFIG) sender_main.c -o reliable_sender $(PTHREAD)
reliable_receiver: receiver_main.c
		gcc $(CONFIG) receiver_main.c -o reliable_receiver $(PTHREAD)
clean:
		rm reliable_sender reliable_receiver

CONFIG = -w -lrt
PTHREAD = -lpthread -lm
