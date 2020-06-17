//package example;
import java.util.function.Function;
import java.util.function.BiFunction;
import java.util.Arrays;
import java.lang.reflect.Constructor;

public inline class x {
    int i = 0;
    public static void main(String[] args) {
        x a = new x();
        for (int i = 0; i < 10000; i++) {
            f0();
            f1();
            f1a();
            f2();
            f3();
        }
    }

    // Static factory initq
    x() { }

    static void f0() {
        Runnable r = () -> System.out.print("");
        r.run();
    }
    static void f1() {
        final String delim = delim();
        Function<String, String> f = (s) -> delim + s + delim;
        String s = f.apply("f1");
//        System.out.println(s);
    }

    static void f1a() {
        final String delim = delim();
        Function<String, String> f = i -> i;
        String s = f.apply("f1");
//        System.out.println(s);
    }

    static void f2() {
        final String delim = delim();
        BiFunction<String, String, String> f2 = (a, b) -> delim + a + "@" + b + delim;
        String s = f2.apply("f1", "f2");
//        System.out.println(s);
    }

    static void f3() {
        final String delim = delim();
        final Object nl = System.lineSeparator();
        BiFunction<String, String, String> f2 = (a, b) -> delim + a + "@" + b + delim + nl;
        String s = f2.apply("f1", "f2");
//        System.out.println(s);
    }

    static String delim() {
        return "|";
    }

    static void t0() {
        Constructor<?>[] c = x.class.getDeclaredConstructors();
        System.out.println("c[]: " + Arrays.toString(c));
    }
}
