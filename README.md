# Homework-Operating-Systems

Distributed Search:
  This program finds all the file names that include the search term within a given directory (and searches in its subdirectories as well) using user-argument amount of threads

Message Slot:
  A mechanism for inter-process communication – Message Slot.
  Message slot is a character device file through which processes communicate using multiple
  message channels. A message slot device can have multiple channels active concurrently, which can
  be used by different processes
  
Server-client architecture:
  This is a toy client/server architecture: a printable characters
  counting server. The client reads N bytes from /dev/urandom and sends it via TCP connection to the server. The server analyze the data and checks how much of the data are printable characters, and sends back the answer to the client
  
Shell:
  A program which acts as a shell-like program, including pipes and background processes and zombie handling
