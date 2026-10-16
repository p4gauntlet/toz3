extern Cell<T> {
    Cell(T initial);
    T get();
}

parser P(out bit<8> result) {
    Cell(8w1) cells[1];
    state start {
        result = cells[0].get();
        transition accept;
    }
}
parser Proto(out bit<8> result);
package Top(Proto p);
Top(P()) main;
