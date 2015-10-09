#ifndef Point2i_H
#define Point2i_H

#include <cmath>
#include <assert.h>

/***************************************************************************
 Point2i.h
 Comment:  This file contains a 2i point definition.
 ***************************************************************************/

//-----------------------------------------------------------------------------
/// Template class for vectors or points with 2 coordinates.
/**
 * This is the main class for all 2i points.
 * The type of the two coordinates is int.
 */
class Point2i {
    
    //-----------------------------------------------------------------------------
public:
    
    /**
     * Standard constructor. Point will be set to 0.
     */
    Point2i()
    : _x( 0 ), _y( 0 ) {};
    
    /**
     * Constructor with given value that will be set to all coordinates.
     * @param v - the value
     */
    Point2i( const int v )
    : _x( v ), _y( v ) {};
    
    /**
     * Constructor with given values for the 2 coordinates.
     * @param x - first coordinate of this point
     * @param y - second coordinate of this point
     */
    Point2i( const int x, const int y )
    : _x( x ), _y( y ) {};
    
    
    /**
     * Returns the first coordinate of this point.
     * @return the \b first coordinate
     */
    int& x() { return _x; };
    
    /**
     * Returns the first coordinate of this point (constant).
     * @return the \b first coordinate
     */
    int x() const { return _x; };
    
    /**
     * Returns the second coordinate of this point.
     * @return the \b second coordinate
     */
    int& y() { return _y; };
    
    /**
     * Returns the second coordinate of this point (constant).
     * @return the \b second coordinate
     */
    int y() const { return _y; };
    
    /**
     * Operator that returns the coordinate at the given index.
     * @param i - index of the coordinate
     * @return the \b coordinate at index \em i
     */
    int& operator [] ( const int i ) {
        assert( i < 2 );
        if ( i == 0 )
            return _x;
        return _y;
    };
    
    /**
     * Operator that returns the coordinate at the given index (constant).
     * @param i - index of the coordinate
     * @return the \b coordinate at index \em i
     */
    int operator [] ( const int i ) const {
        assert( i < 2 );
        if ( i == 0 )
            return _x;
        return _y;
    };
    
    /**
     * Equality operator based on the coordinates of the points.
     * @param p - point to compare with
     * @return \b true if this point is equal to p, else \b false
     */
    bool operator == ( const Point2i& p ) const {
        if ( _x == p.x() && _y == p.y() )
            return true;
        return false;
    };
    
    /**
     * Operator that returns the inverted point.
     * @return the <b> inverted point </b>
     */
    const Point2i operator - () const {
        return Point2i( -_x, -_y );
    };
    
    /**
     * Adding Operator.
     * @param p - the addend
     * @return the \b sum of the points
     */
    const Point2i operator + ( const Point2i& p ) const {
        return Point2i( _x + p.x(), _y + p.y() );
    };
    
    /**
     * Add a point to this point.
     * @param p - the addend
     * @return this point
     */
    Point2i& operator += ( const Point2i& p ) {
        _x += p.x(); _y += p.y();
        return *this;
    };
    
    /**
     * Minus operator.
     * @param p - the subtrahend
     * @return the \b difference point
     */
    const Point2i operator - ( const Point2i& p ) const {
        return Point2i( _x - p.x(), _y - p.y() );
    };
    
    /**
     * Substract a point from this point.
     * @param p - the subtrahend
     * @return this point
     */
    Point2i& operator -= ( const Point2i& p ) {
        _x -= p.x(); _y -= p.y();
        return *this;
    };
    
    /**
     * Division operator for a single value.
     * All coordinates will be divided by the given value.
     * @param w - the divisor
     * @return the <b> new point </b>
     */
    const Point2i operator / ( const int w ) const {
        return Point2i( _x / w, _y / w );
    };
    
    /**
     * Division operator for a single value.
     * All coordinates will be divided by the given value.
     * @param w - the divisor
     * @return the <b> new point </b>
     */
    friend const Point2i operator / ( const int w, const Point2i& p ) {
        return p / w;
    };
    
    /**
     * Divide all coordinates of this point by the given value.
     * @param w - the divisor
     * @return this point
     */
    Point2i& operator /= ( const int w ) {
        _x /= w; _y /= w;
        return *this;
    };
    
    /**
     * Multiply operator for a single value.
     * All coordinates will be multiplied with the given value.
     * @param w - the multiplier
     * @return the <b> new point </b>
     */
    const Point2i operator * ( const int w ) const {
        return Point2i( _x * w, _y * w );
    };
    
    /**
     * Multiply operator for a single value.
     * All coordinates will be multiplied with the given value.
     * @param w - the multiplier
     * @return the <b> new point </b>
     */
    friend const Point2i operator * ( const int w, const Point2i& p ) {
        return p * w;
    };
    
    /**
     * Multiply all coordinates of this point with the given value.
     * @param w - the multiplier
     * @return this point
     */
    Point2i& operator *= ( const int w ) {
        _x *= w; _y *= w;
        return *this;
    };
    
    /**
     * Dot product operator.
     * @param p - another point
     * @return the <b> dot product </b> of the two points
     */
    int operator * ( const Point2i& p ) const {
        return ( _x * p.x() + _y * p.y() );
    };
    
    /**
     * Returns the norm1 of this vector.
     * @return the \b norm1
     */
    int norm1() const {
        return _x + _y;
    };
    
    /**
     * Returns the norm of this vector.
     * @return the \b norm
     */
    double norm() const {
        return std::sqrt( _x * _x + _y * _y);
    };
    
    /**
     * returns the squared norm of this vector
     * @return the <b> squared norm </b>
     */
    int squaredNorm() const {
        return _x * _x + _y * _y;
    };
    
    /**
     * Normalize this point and return a new point with the calculated coordinates.
     * @return the <b> normalized point </b>
     */
    const Point2i normalized() const {
		Point2i normalized;
		const double n = norm();
		normalized.x() = static_cast<int>(std::lround(_x / n));
		normalized.y() = static_cast<int>(std::lround(_y / n));
		return normalized;
    };
    
    /** 
     * Normalize this point. 
     * @return this point
     */
    Point2i& normalize() {
        const double n = norm();
		_x = static_cast<int>(std::lround(_x / n));
		_y = static_cast<int>(std::lround(_y / n));
        return *this;
    };
    
    
    //-----------------------------------------------------------------------------
private:
    
    /** The first coordinate. */
    int _x;
    /** The second coordinate. */
    int _y;
    
};

#endif