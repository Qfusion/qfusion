namespace List {

class Element {
    any value;
    List @list;
    Element @next, prev;

    Element() {
        @list = null;
        @next = null;
        @prev = null;
    }
}

class List {
    int len;
    Element hnode;

    List() {
        len = 0;
        @hnode.prev = @hnode;
        @hnode.next = @hnode;
        @hnode.list = @this;
    }

    ~List() {
        clear();
    }

    void clear() {
        Element @next;

        for( Element @e = @hnode.next; @e != @hnode; @e = @next ) {
            @next = @e.next;
            remove( @e );
        }

        len = 0;
        @hnode.prev = @hnode;
        @hnode.next = @hnode;
        @hnode.list = @this;
    }

    Element @pushBack( any &in value ) {
        Element e;
        @e.list = @this;
       	@e.prev = @hnode;
        @e.next = @hnode.next;
        @e.next.prev = @e;
        @e.prev.next = @e;
        e.value = value;
        
        len++;
        return e;
    }

    Element @front() {
        return @hnode.prev;
    }

    Element @back() {
        return @hnode.next;
    }

    void remove( Element @e ) {
        if( @e is @hnode ) {
            return;
        }
        if( @e.list !is @this ) {
            return;
        }
        
        @e.list = null;
        @e.prev.next = @e.next;
        @e.next.prev = @e.prev;
        len--;
    }

    Element @popFront() {
        Element @e = front();
        remove( @e );
        return e;
    }

    Element @popBack() {
        Element @e = back();
        remove( @e );
        return e;
    }

    bool empty() {
        return len == 0;
    }
}


}
