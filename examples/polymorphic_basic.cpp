// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/indirect/polymorphic.hpp>

#include <iostream>
#include <string>

struct Shape {
    virtual ~Shape()                 = default;
    virtual double      area() const = 0;
    virtual std::string name() const = 0;
    Shape()                          = default;
    Shape(const Shape&)              = default;
    Shape(Shape&&)                   = default;
    Shape& operator=(const Shape&)   = default;
    Shape& operator=(Shape&&)        = default;
};

struct Circle : Shape {
    double radius_;
    explicit Circle(double r) : radius_(r) {}
    double      area() const override { return 3.14159265 * radius_ * radius_; }
    std::string name() const override { return "Circle"; }
};

struct Rectangle : Shape {
    double w_, h_;
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double      area() const override { return w_ * h_; }
    std::string name() const override { return "Rectangle"; }
};

int main() {
    // polymorphic owns a heap-allocated derived object with polymorphic deep-copy.
    beman::indirect::polymorphic<Shape> shape(Circle(5.0));
    std::cout << shape->name() << " area: " << shape->area() << "\n";

    // Copy preserves the dynamic type.
    auto copy = shape;
    std::cout << "Copy is a " << copy->name() << " with area " << copy->area() << "\n";

    // Can hold different derived types at different times.
    shape = beman::indirect::polymorphic<Shape>(Rectangle(3.0, 4.0));
    std::cout << shape->name() << " area: " << shape->area() << "\n";

    return 0;
}
