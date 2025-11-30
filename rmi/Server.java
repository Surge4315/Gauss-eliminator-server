import java.rmi.registry.LocateRegistry;
import java.rmi.registry.Registry;

public class Server {
    public static void main(String[] args) {
        try {
             Gauss gauss = new GaussImpl();

            // Utworzenie lokalnego rejestru RMI na porcie 1099
            Registry registry = LocateRegistry.createRegistry(1099);

            // Rejestrujemy obiekt zdalny pod nazwą "GaussService"
            registry.rebind("GaussService", gauss);

            System.out.println("Serwer RMI działa...");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
