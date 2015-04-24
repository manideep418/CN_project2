
How to Build
------------

Before building the project, make sure that the machine has 
a 'gcc' compiler and 'make' utility installed.

To build the project, execute:
 cd PROJECT_ROOT/bin  
 make zlib
 make


How to Run
----------

To launch the server, use the following command:
 cd PROJECT_ROOT/bin
 ./server <PORT NUMBER> <SITES_BLOCKLIST> <WORDS_FILTER> <CACHE_DIRECTORY>

For example, after building the project I can run it with the command:
./bin/server 8888 ./blocklist.txt ./filter_words.txt ./cache


Options
-------

The meaning of all the arguments is such:

<PORT NUMBER> - port that the proxy will listen to
<SITES_BLOCKLIST> - a path to a text file that contains one line per blocked site, e.g.:
google.com
www.google.com
youtube.com
www.youtube.com
... and so on ...

When a user tries to access one of these sites, he will see "Access Denied" message in his browser

<WORDS_FILTER> - a path to a text file that contains one line per a filtered word. 
All such words on the page will be replaced by "CENSORED" string

<CACHE_DIRECTORY> - path to directory where proxy will store cached responses.


Testing
-------

After the server starts, you can test it in your browser like so:

http://localhost:<PORT NUMBER>/www.thehindu.com
http://localhost:<PORT NUMBER>/www.hyperhero.com/en/insults.htm
