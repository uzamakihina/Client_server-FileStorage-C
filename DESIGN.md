 keep in memory `allFiles`, an array of `struct fileStruct` objects, such that each
`fileStruct` object contains a `hashname` that represents the file contents (almost) uniquely,
the number of aliases it has as `numAliases`, and also a variable-length array of character pointers as `knownas` that tracks all the file names associated to the contents of the file via its hash (the variable length allows a particular file to have a flexible amount of aliases).

On the server side, a new thread is created to handle each client, ensuring the traffic the client creates is handled efficiently by one unit. Semaphore waiting/signalling with `sem_t mutex` is placed strategically within the code to increase concurrency (while maintaining integrity), and a simple semaphore strategy is used to reduce complexity and improve efficiency.

As mentioned above,  use semaphores to allow different threads to have mutual exclusion. `allFiles` is only accessed by threads if they have a lock on `mutex`. Changes must therefore be consistent since only one thread is allowed access to our main data structure at any time.

simply store the contents in memory as a character array `fileContents` before determining if it matches another `hashname` and needs to be saved on the server. This will undoubtedly not scale very well.

The separation of the threads along with the usage of `mutex` will keep a buggy client from harming the rest of the server when it is acting up. Also, the server and client code is rigid enough (i.e. client only having `l` `u` `d` `r` `q` as command options, client can only send even-numbered hex codes, server only sends odd-numbered hex codes) that improper inputs will likely cause the application to ignore the input, or in the worst case, become stuck.

When a `SIGTERM` is issued, the server has a `signalHandler` that `pthread_cancel`s all the threads, then `pthread_join`s them. The thread that is currently accessing `allFiles` is not able to be cancelled until it finishes its task (this ensures `allFiles` cannot be in an inconsistent state upon exiting), at which point it will be canceled and joined. Then, the main thread closes its socket, and saves the contents of `allFiles` via `writeFile` from memory into the `.dedup` file before finally exiting. This ensures a successful boot of the server next time.

 make sure the `.dedup` file exists, and if it exists, then make sure that it is formatted according to our expectations using `readFile`.
Other files being in the same directory do not effect the startup of the server. If there are other files with filenames that are not hashes or `.dedup`, they will never interact with the application. If there are badly formatted `.dedup` or files with hashes as names, then they may be overwritten by the application, but the application will not otherwise be affected.

Zero sized files are handled in a special way, as functions on both the server and client side have `if` statements that will return early if the size is `0` on either side (terminating after sending/receiving the `size` data block). Zero sized files still have hashes like non-empty files, and are stored appropriately with their corresponding aliases and hashname.
