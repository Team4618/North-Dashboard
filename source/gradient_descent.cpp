//Dual Numbers ------------------------------------------------

//Represents the dual number a + b*epsilon
struct dualNumber {   
   f32 a;
   f32 b;
};

dualNumber DualNumber(f32 a, f32 b) {
   dualNumber result = {a, b}; 
   return result;
}

dualNumber operator- (dualNumber a) {
   return DualNumber(-a.a, -a.b);
}

dualNumber operator+ (dualNumber a, dualNumber b) {
   return DualNumber(a.a + b.a,
                     a.b + b.b);
}

dualNumber operator- (dualNumber a, dualNumber b) {
   return DualNumber(a.a - b.a,
                     a.b - b.b);
}

dualNumber operator* (dualNumber a, dualNumber b) {
   return DualNumber(a.a * b.a,
                     a.a * b.b + a.b * b.a);
}

dualNumber operator/ (dualNumber a, dualNumber b) {
   //TODO: the case where a.a & b.a are zero isnt handled properly
   return DualNumber(a.a / b.a,
                     (a.b * b.a - a.a * b.b) / (b.a * b.a));
}

//f(a+b*epsilon) = f(a) + f'(a)*b*epsilon
dualNumber sin(dualNumber x) {
   return DualNumber(sin(x.a), cos(x.a)*x.b);
}

dualNumber cos(dualNumber x) {
   return DualNumber(cos(x.a), -sin(x.a)*x.b);
}

//TODO: do we want to be able to change number of parameters at runtime?

//Optimizers   ------------------------------------------------
template<u32 N, typename T>
struct Vector {
   T e[N];
};

template<u32 N, typename T>
Vector<N, T> operator+ (Vector<N, T> a, Vector<N, T> b) {
	Vector<N, T> output = {};
   
   for(u32 i = 0; i < N; i++) {
      output[i] = a.e[i] + b.e[i];
   }

	return output;
}

template<u32 N, typename T>
struct Boundary {
   T maxs[N];
   T mins[N];
};

template<u32 N, typename T>
bool Contains(Boundary<N, T> bounds, Vector<N, T> x) {
   bool result = true;
   
   for(u32 i = 0; i < N; i++) {
      result &= (bounds.mins[i] <= x.e[i]) && (x.e[i] <= bounds.maxs[i]);
   }

   return result;
}

struct FunctionBoundaries {
   f32 *maxs;
   f32 *mins;
   u32 count;
};

struct FunctionParameters {
   f32 *vals;
   u32 count;
};

typedef f32 (* FunctionCallback)(FunctionParameters params);

// template<u32 N, template<typename T> typename func> 
FunctionParameters GradientDescent(FunctionCallback func, FunctionBoundaries boundaries, 
                                   u32 start_sample_tries = 10, u32 max_descent_tries = 100, 
                                   f32 convergence_threshold = 0.01)
{
   FunctionParameters params = {};
   
   //Finds a some starting params by random sampling 
   f32 curr_max = -F32_MAX;
   for(u32 i = 0; i < start_sample_tries; i++) {
      FunctionParameters random_params = {}; //Generate random params within boundaries
      f32 val = func(random_params);

      if(val > curr_max) {
         curr_max = val;
         params = random_params;
      }
   }
   
   for(u32 i = 0; i < max_descent_tries; i++) {
      f32 gradient = 0; //Get gradient of "func" at "params"

      f32 scale_coeff = 1;
      // params = params + scale_coeff*gradient;

      // if(LengthSq(gradient) < convergence_threshold) {
      //    break;
      // }
   }

   return params;
}

//-------------------------------
template<typename T>
T test_func(Vector<2, T> params) {
   T x = param.e[0];
   T y = param.e[1];
   
   return x*x + y*y;
} 

// FunctionCallback test_func_callback = test_func<f32>;