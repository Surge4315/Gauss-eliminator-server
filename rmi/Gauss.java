import java.rmi.Remote;
import java.rmi.RemoteException;

public interface Gauss extends Remote {
    String sayHello(String name) throws RemoteException;

    double[][] gaussianElimination(double[][] matrix) throws RemoteException;
}
