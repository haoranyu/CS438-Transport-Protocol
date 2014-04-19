all: reliable_sender reliable_receiver
reliable_sender: sender_main.c
		gcc -w -o reliable_sender sender_main.c
reliable_receiver: receiver_main.c
		gcc -w -o reliable_receiver receiver_main.c
clean:
		rm eliable_sender reliable_receiver