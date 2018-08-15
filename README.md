/* CSci4061 S2017 Assignment 1
* date: 05/02/17
* name: Sushmitha Ayyanar, Christopher Patterson, Wei Chen
* id: ayyan003, patte539, chen4626*/

* Sushmitha implemented the dispatch thread function and main function with Christopher and documentation.    
* Christopher implemented the dispatch, worker function and main function and debug them, i.e., everything except the log_request.      
* Wei implemented his own version entire project 4 from scratch which includes his own version dispatch, worker, insert_queue, push_queue, log_request function.  
He performed the concurrency test and error handling as well as the documentation.              


How to run the program:           
make        
In server terminal: ./web_server 9000 '/path/CSCI_4061_Project4/testing' 100 100 100           
In client terminal:             
wget http://127.0.0.1:9000/image/jpg/29.jpg                 
wget -i '/path/CSCI_4061_Project4/testing/urls' -O myres      
echo "$(cat testing/urls)" | xargs -n 1 -P 64 wget      


How the program works:          
Implemented multithreaded web server using dispatcher threads and worker threads and kept the request queue synchronized via conditional variable.      
Three components:               
1) main thread creates a number of dispatch threads and worker threads.         
2) dispatch thread function repeatedly accepts an incoming connection, reads the request from the connection and places the
request in the queue via insert_queue function, which uses conditional variable to ensure single dispatch thread can insert.          
3) worker thread function to repeatedly polling the request queue, pick up request from it via push_queue function, which
uses conditional variable to ensure only single worker thread can push. Then worker thread would serve the request back to client
and log the request.            
