#pragma once 



namespace p2p {

    template <typename T> class static_vector
    {
        std::vector<T*> v;
    private:
        // no copy no move.
        static_vector(const static_vector<T> &) = delete;
        static_vector(static_vector<T> && ) = delete;
    public:
        using iterator = std::vector<T*>::const_iterator;

        static_vector() { }
        static_vector(std::initializer_list<T*> l)
        :v(l)
        {

        }
        ~static_vector() {
            for (T *i: v)
            {
                delete i;
            }
        }
        void push_back(T*value) { v.push_back(value);}

        iterator begin() const { return v.begin(); }
        iterator end() const { return v.end(); }

        T*operator[](size_t index) const { return v[index]; }

        size_t size() const { return v.size(); }
    };
}