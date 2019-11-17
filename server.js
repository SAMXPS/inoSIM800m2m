var net = require('net');
const fs = require('fs');

var server = net.createServer(function(socket) {
	socket.write('Echo server\r\n');
	socket.pipe(socket);
});

server.listen(25565);

server.on('connection', function(socket) {
    console.log('A new connection has been established.');

    socket.write('Hello, client.');

    socket.on('data', function(chunk) {
        console.log(`Data received from client: ${chunk.toString()}.`);

        var a = chunk.toString();
        a.indexOf("+CIPGSMLOC")
        fs.appendFile('log.txt', a, function (err) {
            if (err) throw err;
            console.log('Saved!');
        });
    });

    socket.on('end', function() {
        console.log('Closing connection with the client');
    });

    socket.on('error', function(err) {
        console.log(`Error: ${err}`);
    });
});