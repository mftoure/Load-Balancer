package src;


import java.util.ArrayList;
import java.util.List;


public class Initialization {

	private final static int N = 4;									// number of servers
	private final static int initialPort = 7000;					//starting point of servers 's port
	private final static List<Server> servers= new ArrayList<>();	// list of servers
	
	
	//initiate servers with their ports and add them to the list of servers
	public static void initiateServers() {
		int port = initialPort ;
		
		for (int i = 0 ; i < N ; i++) {
			Server s = new Server(port);
			servers.add(s);
			port ++;
		}
	}
	
	
	// connect servers to one another
	// we'll have N*(N-1)/2 connections (sockets)
	public static void connectServers() {
		for (Server s1 : servers) {
			for (Server s2: servers) {
				// we make sure that we won't have any duplicates given that a connection is bidirectional
				if(s2.getId() > s1.getId()) {
					s2.connectTo(s1);
				}
			}
		}
	}
		
	

	public static void main(String args[]) {
		
		initiateServers();
		connectServers();
		
	}
}
