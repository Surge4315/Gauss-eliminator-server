import java.rmi.RemoteException;
import java.rmi.server.UnicastRemoteObject;

public class GaussImpl extends UnicastRemoteObject implements Gauss {

    public GaussImpl() throws RemoteException {
        super();
    }

    @Override
    public String sayHello(String name) throws RemoteException {
        return "Witaj, " + name + "!";
    }

    // --- helper: zamiana wierszy ---
    private void swapRow(double[][] mat, int i, int j) {
        double[] tmp = mat[i];
        mat[i] = mat[j];
        mat[j] = tmp;
    }

    // --- forward elimination ---
    private int forwardElim(double[][] mat) {
        int N = mat.length;

        for (int k = 0; k < N; k++) {
            int iMax = k;
            double vMax = Math.abs(mat[iMax][k]);

            for (int i = k + 1; i < N; i++) {
                if (Math.abs(mat[i][k]) > vMax) {
                    vMax = Math.abs(mat[i][k]);
                    iMax = i;
                }
            }

            if (Math.abs(mat[iMax][k]) < 1e-9)
                return k;

            if (iMax != k)
                swapRow(mat, k, iMax);

            for (int i = k + 1; i < N; i++) {
                double f = mat[i][k] / mat[k][k];
                for (int j = k + 1; j <= N; j++)
                    mat[i][j] -= mat[k][j] * f;
                mat[i][k] = 0;
            }
        }
        return -1;
    }

    // --- back substitution ---
    private void backSub(double[][] mat) {
        int N = mat.length;
        double[] x = new double[N];

        for (int i = N - 1; i >= 0; i--) {
            x[i] = mat[i][N];
            for (int j = i + 1; j < N; j++)
                x[i] -= mat[i][j] * x[j];
            x[i] /= mat[i][i];
        }
        //for(int i=0; i<N; i++) {
        //    System.out.printf("%.4f\n", x[i]);
        // }
    }

    @Override
    public double[][] gaussianElimination(double[][] mat) throws RemoteException {

        System.out.println("\nrozmiar: " + mat.length);
        long startWall = System.nanoTime();
        long startCpu = System.nanoTime();  // uproszczenie — w Javie dokładny CPU time wymaga MXBean

        int singular = forwardElim(mat);

        if (singular != -1) {
            System.out.println("Macierz osobliwa — układ może nie mieć jednoznacznego rozwiązania.\n");
            return mat;
        }

        backSub(mat);

        long endWall = System.nanoTime();
        long endCpu = System.nanoTime();

        double wallSeconds = (endWall - startWall) / 1e9;
        double cpuSeconds = (endCpu - startCpu) / 1e9;

        System.out.println("szeregowo:");
        System.out.println("Czas zegarowy (wall): " + wallSeconds + " s");
        System.out.println("Czas CPU: " + cpuSeconds + " s");

        return mat;
    }
}

