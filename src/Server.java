package src;


import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;

public class Server {

	private int port;
	private int id;
	private static int cpt = 0;
	
	public Server(int port){
		
		try {
			this.port = port;
			ServerSocket server = new ServerSocket(port);
			server.accept();
			this.id = cpt ++;
		}catch(IOException e) {
			e.printStackTrace();;
		}
		
	}
	
	public int getPort() {
		return this.port;
	}
	
	public int getId() {
		return this.id;
	}
	
	
	public void connectTo(Server s2) {
		
		
	}
}
