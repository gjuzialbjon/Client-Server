# Client-Server

A	multi-threaded server	that processes keyword search	queries	from clients.
Multiple clients can request service	from the server	concurrently. Communication occurs in a shared memory segment.	
Server gathers request from the clients and serves them immediately by creating threads to find the lines matching the 
keyword, and sending it to the client by using semaphores in order to provide a better communication.
