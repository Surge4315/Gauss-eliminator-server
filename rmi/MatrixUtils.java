import java.util.Random;

public class MatrixUtils {

    private static Random rand = new Random();

    // Funkcja generująca macierz diagonalnie dominującą
    public static double[][] generateDiagonallyDominantMatrix(int n) {
        double[][] A = new double[n][n];

        for (int i = 0; i < n; i++) {
            double sum = 0;

            for (int j = 0; j < n; j++) {
                if (i != j) {
                    A[i][j] = rand.nextInt(10); // 0–9
                    sum += Math.abs(A[i][j]);
                }
            }

            A[i][i] = sum + rand.nextInt(10) + 1;
        }

        return A;
    }

    // Funkcja generująca wektor wyrazów wolnych
    public static double[] generateVectorB(int n) {
        double[] b = new double[n];
        for (int i = 0; i < n; i++)
            b[i] = rand.nextInt(20); // 0–19
        return b;
    }

    // Stworzenie macierzy rozszerzonej (A|b)
    public static double[][] createAugmentedMatrix(double[][] A, double[] b) {
        int N = A.length;
        double[][] Ab = new double[N][N + 1];

        for (int i = 0; i < N; i++) {
            System.arraycopy(A[i], 0, Ab[i], 0, N);
            Ab[i][N] = b[i];
        }

        return Ab;
    }

    // Wypisywanie macierzy rozszerzonej
    public static void printSystemAug(double[][] Ab) {
        int n = Ab.length;
        int m = Ab[0].length;

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m - 1; j++) {
                System.out.printf("%.4f ", Ab[i][j]);
            }
            System.out.printf("| %.4f\n", Ab[i][m - 1]);
        }
    }

    // Wypisywanie macierzy A i wektora b
    public static void printSystem(double[][] A, double[] b) {
        int n = A.length;

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                System.out.print(A[i][j] + "\t");
            }
            System.out.println("| " + b[i]);
        }
    }

    public static void printGaussSolutions(double[][] mat) {
        int N = mat.length;
        double[] x = new double[N];

        // odwrotne podstawianie
        for (int i = N - 1; i >= 0; i--) {
            x[i] = mat[i][N]; // ostatnia kolumna
            for (int j = i + 1; j < N; j++) {
                x[i] -= mat[i][j] * x[j];
            }
            x[i] /= mat[i][i];
        }

        // wypisanie rozwiązań
        for (int i = 0; i < N; i++) {
            System.out.printf("x%d = %.4f\n", i + 1, x[i]);
        }
    }
}