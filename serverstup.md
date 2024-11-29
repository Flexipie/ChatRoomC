ssh -i Chat-Server-EC2.pem ec2-user@44.202.241.190
gcc server.c -o server -pthread
./server


gcc client.c -o client -pthread
./client public ipv4
