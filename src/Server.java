package src;


import java.io.IOException;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.HashMap;
import java.util.Map;

public class Server {
	
	private String host;
	private int port;
	private int id;
	private static int cpt = 0;
	private Map<Integer, Socket> connexions = new HashMap<Integer, Socket>();
	
	public Server(int port){
		
		try {
			this.port = port;
			ServerSocket server = new ServerSocket(port);
			server.accept();
			this.id = cpt ++;
			this.host = "ppti−14−508−0"+id+".ufr−info−p6.jussieu.fr";
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
	
	public String getHost() {
		return this.host;
	}
	
	
	public void connectTo(Server s2) {
		
		try {
			InetAddress inetAddress = InetAddress.getByName(s2.getHost());
			Socket socket = new Socket (inetAddress, s2.getPort() );
			connexions.put(s2.id, socket);
			
		}catch(Exception e) {
			e.printStackTrace();
		}
		
	}
}
